/* IrisSNN v2 — improved neuromorphic engine: same LIF neurons, same runtime-loaded
 * weights/thresholds/coding, but FOUR sample-parallel neuron populations (spatial
 * replication — the neuromorphic analog of multi-core) stepping in lockstep, and a
 * higher closed clock. Per-sample spike counts are independent, so the checksum golden
 * from v1 must reproduce exactly.
 */
#include <stdint.h>

#define SNN_P 4
#define SNN_CFG_W 96
#define SNN_XMEM_B 1024
#define SNN_YMEM_B 256
#define CFG_T 0
#define CFG_W1 8
#define CFG_B1 40
#define CFG_TH1 48
#define CFG_W2 56
#define CFG_B2 80
#define CFG_TH2 83

void iris_snn(int reps, int n_samp,
              int32_t cfg[SNN_CFG_W], int8_t xmem[SNN_XMEM_B], int8_t ymem[SNN_YMEM_B],
              uint32_t *cs_out, int *acc_out)
{
#pragma HLS interface s_axilite port=reps    bundle=ctrl
#pragma HLS interface s_axilite port=n_samp  bundle=ctrl
#pragma HLS interface s_axilite port=cfg     bundle=ctrl
#pragma HLS interface s_axilite port=xmem    bundle=ctrl
#pragma HLS interface s_axilite port=ymem    bundle=ctrl
#pragma HLS interface s_axilite port=cs_out  bundle=ctrl
#pragma HLS interface s_axilite port=acc_out bundle=ctrl
#pragma HLS interface s_axilite port=return  bundle=ctrl

    int32_t c_[SNN_CFG_W];
#pragma HLS array_partition variable=c_ complete
    for(int i=0;i<SNN_CFG_W;i++){
#pragma HLS pipeline II=1
        c_[i]=cfg[i]; }
    int T=c_[CFG_T];

    uint32_t cs=0; int good=0;
REPS:
    for(int r=0;r<reps;r++){
#pragma HLS loop_tripcount min=1 max=100000
        cs=0x811c9dc5u; good=0;
GROUPS:
        for(int g=0;g<(n_samp+SNN_P-1)/SNN_P;g++){
#pragma HLS loop_tripcount min=38 max=38
            int32_t xi[SNN_P][4], ax[SNN_P][4], V[SNN_P][8], U[SNN_P][3], cnt[SNN_P][3];
#pragma HLS array_partition variable=xi complete dim=0
#pragma HLS array_partition variable=ax complete dim=0
#pragma HLS array_partition variable=V complete dim=0
#pragma HLS array_partition variable=U complete dim=0
#pragma HLS array_partition variable=cnt complete dim=0
            for(int p=0;p<SNN_P;p++){
#pragma HLS unroll
                int s=g*SNN_P+p; int sc=(s<n_samp)?s:0;
                for(int k=0;k<4;k++){
#pragma HLS unroll
                    xi[p][k]=(int32_t)xmem[4*sc+k]+17; ax[p][k]=128; }
                for(int j=0;j<8;j++) V[p][j]=0;
                for(int o=0;o<3;o++){ U[p][o]=0; cnt[p][o]=0; }
            }
TSTEPS:
            for(int t=0;t<T;t++){
#pragma HLS pipeline II=2
#pragma HLS loop_tripcount min=128 max=128
                for(int p=0;p<SNN_P;p++){
#pragma HLS unroll
                    int32_t sx[4];
                    for(int k=0;k<4;k++){
#pragma HLS unroll
                        int32_t m=xi[p][k]<0?-xi[p][k]:xi[p][k];
                        ax[p][k]+=m;
                        int fire=(ax[p][k]>=256)?1:0;
                        if(fire) ax[p][k]-=256;
                        sx[k]=fire?(xi[p][k]<0?-1:1):0; }
                    int32_t hs[8];
                    for(int j=0;j<8;j++){
#pragma HLS unroll
                        int32_t cur=c_[CFG_B1+j];
                        for(int k=0;k<4;k++){
#pragma HLS unroll
                            if(sx[k]) cur+=sx[k]*(c_[CFG_W1+4*j+k]<<8); }
                        int32_t v=V[p][j]+cur;
                        int32_t th=c_[CFG_TH1+j];
                        int f=(v>=th)?1:0;
                        hs[j]=f; V[p][j]=f?(v-th):v; }
                    for(int o=0;o<3;o++){
#pragma HLS unroll
                        int32_t cur=c_[CFG_B2+o];
                        for(int j=0;j<8;j++){
#pragma HLS unroll
                            if(hs[j]) cur+=c_[CFG_W2+8*o+j]<<8; }
                        int32_t u=U[p][o]+cur;
                        int32_t th=c_[CFG_TH2+o];
                        if(u>=th){ cnt[p][o]++; u-=th; }
                        U[p][o]=u; }
                }
            }
            for(int p=0;p<SNN_P;p++){
                int s=g*SNN_P+p;
                if(s<n_samp){
                    int best=0;
                    for(int o=0;o<3;o++){
                        uint8_t b=(uint8_t)cnt[p][o];
                        cs=(cs^(uint16_t)b)*16777619u;
                        if(cnt[p][o]>cnt[p][best]) best=o;
                    }
                    if(best==ymem[s]) good++;
                }
            }
        }
    }
    *cs_out=cs; *acc_out=good;
}
