set part xczu7ev-ffvc1156-2-e
set wd [pwd]
create_project snn_prj $wd/snn_prj -part $part -force
set_property ip_repo_paths [list $wd/ip_iris_snn/hls/impl/ip] [current_project]
update_ip_catalog

create_bd_design "snn_bd"
# --- Zynq UltraScale+ PS (part defaults: HPM0 FPD master + pl_clk0) ---
set ps [create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ps]
apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "0"} $ps
set_property -dict [list \
  CONFIG.PSU__USE__M_AXI_GP0 {1} \
  CONFIG.PSU__USE__M_AXI_GP2 {0} \
  CONFIG.PSU__FPGA_PL0_ENABLE {1} \
  CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100} \
] $ps
# --- MXU IP ---
set mxu [create_bd_cell -type ip -vlnv xilinx.com:hls:iris_snn:1.0 iris_snn_0]
# --- connect s_axi_ctrl to PS HPM0 FPD (auto SmartConnect + reset + clock) ---
apply_bd_automation -rule xilinx.com:bd_rule:axi4 \
  -config { Master {/zynq_ps/M_AXI_HPM0_FPD} Clk {Auto} } \
  [get_bd_intf_pins iris_snn_0/s_axi_ctrl]
assign_bd_address
validate_bd_design
regenerate_bd_layout
save_bd_design
puts "=== ADDRESS MAP ==="
foreach a [get_bd_addr_segs] { puts "$a : [get_property OFFSET $a] size [get_property RANGE $a]" }

# --- wrapper + implement ---
set bd [get_files *snn_bd.bd]
make_wrapper -files $bd -top
add_files -norecurse [glob $wd/snn_prj/snn_prj.gen/sources_1/bd/snn_bd/hdl/snn_bd_wrapper.v]
set_property top snn_bd_wrapper [current_fileset]
update_compile_order -fileset sources_1
launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
puts "IMPL_STATUS [get_property STATUS [get_runs impl_1]]"
# --- collect outputs ---
file copy -force [glob $wd/snn_prj/snn_prj.runs/impl_1/snn_bd_wrapper.bit] $wd/snn.bit
file copy -force [glob $wd/snn_prj/snn_prj.gen/sources_1/bd/snn_bd/hw_handoff/*.hwh] $wd/snn.hwh
puts "BD_BUILD_DONE bit=[file exists $wd/snn.bit] hwh=[file exists $wd/snn.hwh]"
# timing summary
open_run impl_1
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
puts "WNS=$wns"
