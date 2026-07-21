#!/usr/bin/env bash
# Deploy the IrisSNN to the ZCU104, load the model at runtime, run the test suite.
# Usage: deploy_snn.sh <bit-basename e.g. snn|snn_v2> <tag> <mhz: 100|125|150|187|214|250>
set -e
cd /opt/jupyterlab/pl_dpu/snn-iris
BIT=${1:-snn}; TAG=${2:-v1}; MHZ=${3:-100}
case $MHZ in
  100) DIVVAL=0x01010F00;;
  125) DIVVAL=0x01010C00;;
  150) DIVVAL=0x01010A00;;
  187) DIVVAL=0x01010800;;
  214) DIVVAL=0x01010700;;
  250) DIVVAL=0x01010600;;
  *) echo "unsupported MHz"; exit 1;;
esac
PSPW='Edupassword1_'
SSH="sshpass -p $PSPW ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SCP="sshpass -p $PSPW scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

source /opt/Xilinx/2025.2/Vitis/settings64.sh
cat > $BIT.bif <<EOF
all:
{
    [destination_device = pl] $BIT.bit
}
EOF
bootgen -arch zynqmp -image $BIT.bif -w -o ${BIT}_boot.bin -process_bitstream bin > bootgen_snn.log 2>&1
ls -l $BIT.bit.bin

# register map header from the HLS export used for this bitstream
HW_H=$(find ip_iris_snn -name "xiris_snn_hw.h" -path "*drivers*" | head -1)
cp "$HW_H" host/xiris_snn_hw.h

$SSH ubuntu@zynqmp "mkdir -p ~/snn"
$SCP $BIT.bit.bin host/snn_host.c host/snn_config.h host/iris_model.h host/xiris_snn_hw.h  ubuntu@zynqmp:~/snn/
$SSH ubuntu@zynqmp "set -e; cd ~/snn
  gcc -O2 -I. -o snn_host snn_host.c
  sudo fpgautil -b $BIT.bit.bin
  sleep 1
  sudo busybox devmem 0xFF5E00C0 32 $DIVVAL
  echo PL0 now: \$(sudo busybox devmem 0xFF5E00C0)  # ${MHZ} MHz
  sudo timeout 600 ./snn_host $TAG $MHZ 2>&1 | tee snn_${TAG}_${MHZ}.log
  sudo busybox devmem 0xFF5E00C0 32 0x01010F00
  echo PL0 restored: \$(sudo busybox devmem 0xFF5E00C0)
  sudo cp board_results.json board_results_${TAG}_${MHZ}.json"
$SCP ubuntu@zynqmp:~/snn/board_results_${TAG}_${MHZ}.json results/snn_${TAG}_${MHZ}_board_results.json
$SCP ubuntu@zynqmp:~/snn/snn_${TAG}_${MHZ}.log results/snn_${TAG}_${MHZ}_board.log
echo DEPLOY_SNN_DONE
