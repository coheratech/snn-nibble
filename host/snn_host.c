#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include "snn_config.h"
#include "xiris_snn_hw.h"
#define SNN_GOLD_CS 0xea6d8757u
static volatile uint32_t *reg;
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void wr(uint32_t o,uint32_t v){reg[o/4]=v;}
static uint32_t rd(uint32_t o){return reg[o/4];}
static void wrb(uint32_t base,const int8_t*b,int n){for(int i=0;i<n;i+=4){uint32_t w=0;for(int j=0;j<4&&i+j<n;j++)w|=((uint32_t)(uint8_t)b[i+j])<<(8*j);wr(base+i,w);}}
static double run(int reps,uint32_t*cs,int*acc){
    while(!(rd(XIRIS_SNN_CTRL_ADDR_AP_CTRL)&4));
    wr(XIRIS_SNN_CTRL_ADDR_REPS_DATA,(uint32_t)reps);
    __sync_synchronize(); wr(XIRIS_SNN_CTRL_ADDR_AP_CTRL,1); __sync_synchronize();
    int sb=0; double t0=now();
    while(1){uint32_t v=rd(XIRIS_SNN_CTRL_ADDR_AP_CTRL); if(!(v&4))sb=1; else if(sb)break; if(now()-t0>300)break;}
    double dt=now()-t0;
    *cs=rd(XIRIS_SNN_CTRL_ADDR_CS_OUT_DATA); *acc=(int)rd(XIRIS_SNN_CTRL_ADDR_ACC_OUT_DATA);
    return dt;
}
int main(int argc,char**argv){
    const char*tag=argc>1?argv[1]:"v1"; int clk=argc>2?atoi(argv[2]):100;
    int fd=open("/dev/mem",O_RDWR|O_SYNC);
    reg=(volatile uint32_t*)mmap(0,0x10000,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0xA0000000);
    snn_image_t im; snn_build_iris(&im,128);
    for(int i=0;i<SNN_CFG_W;i++) wr(XIRIS_SNN_CTRL_ADDR_CFG_BASE+4*i,(uint32_t)im.cfg[i]);
    wrb(XIRIS_SNN_CTRL_ADDR_XMEM_BASE,im.xmem,SNN_XMEM_B);
    wrb(XIRIS_SNN_CTRL_ADDR_YMEM_BASE,im.ymem,SNN_YMEM_B);
    wr(XIRIS_SNN_CTRL_ADDR_N_SAMP_DATA,(uint32_t)im.n_samp);
    uint32_t cs; int acc; int fails=0;
    for(int i=0;i<3;i++){ run(1,&cs,&acc);
        int ok=(cs==SNN_GOLD_CS)&&(acc==147);
        printf("infer%d cs=0x%08x acc=%d/150 %s\n",i,cs,acc,ok?"MATCH":"DIFF"); fflush(stdout); if(!ok)fails++; }
    double dt=run(50,&cs,&acc);
    int reps=(int)(2.0/(dt/50)); if(reps<50)reps=50; if(reps>1000000)reps=1000000;
    dt=run(reps,&cs,&acc);
    int ok=(cs==SNN_GOLD_CS)&&(acc==147); if(!ok)fails++;
    double us=dt/((double)reps*150)*1e6;
    printf("TIMING reps=%d dt=%.4fs  %.3f us/inf  %.3e inf/s %s\n",reps,dt,us,(double)reps*150/dt,ok?"MATCH":"DIFF");
    printf(fails?"SNN_FAIL\n":"SNN_PASS\n");
    FILE*f=fopen("board_results.json","w");
    fprintf(f,"{\n \"variant\": \"%s\",\n \"clk_mhz\": %d,\n \"T\": 128,\n \"cs\": \"0x%08x\",\n \"acc\": %d,\n \"us_per_inf\": %.4f,\n \"inf_per_s\": %.1f,\n \"weights_thresholds_loaded_at_runtime\": true,\n \"pass\": %s\n}\n",tag,clk,cs,acc,us,(double)reps*150/dt,fails?"false":"true");
    fclose(f); return fails?1:0;
}
