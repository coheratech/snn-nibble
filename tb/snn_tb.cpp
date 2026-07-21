#include <stdio.h>
#include <stdint.h>
#include "snn_config.h"
void iris_snn(int reps,int n_samp,int32_t cfg[SNN_CFG_W],int8_t xmem[SNN_XMEM_B],
              int8_t ymem[SNN_YMEM_B],uint32_t *cs_out,int *acc_out);
int main(int argc,char**argv){
    for(int T=32;T<=512;T*=2){
        snn_image_t im; snn_build_iris(&im,T);
        uint32_t cs; int acc;
        iris_snn(1,im.n_samp,im.cfg,im.xmem,im.ymem,&cs,&acc);
        uint32_t cs2; int acc2;
        iris_snn(1,im.n_samp,im.cfg,im.xmem,im.ymem,&cs2,&acc2);
        printf("T=%3d acc=%d/150 cs=0x%08x det=%s\n",T,acc,cs,(cs==cs2&&acc==acc2)?"yes":"NO");
    }
    return 0;
}
