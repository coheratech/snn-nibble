#include <stdio.h>
#include <stdint.h>
#include "snn_config.h"
// exact int8 MLP reference (irisbench semantics)
static int32_t srdhm(int32_t a,int32_t b){int64_t ab=(int64_t)a*b;int64_t n=(ab>=0)?(1LL<<30):(1LL-(1LL<<30));return (int32_t)((ab+n)>>31);}
static int32_t rdbps(int32_t x,int32_t s){if(!s)return x;int32_t m=(1<<s)-1,r=x&m,t=(m>>1)+(x<0);return (x>>s)+(r>t);}
static int8_t rq(int32_t a,int32_t m0,int32_t sh,int32_t zp){int32_t v=rdbps(srdhm(a,m0),sh)+zp;if(v<-128)v=-128;if(v>127)v=127;return v;}
void iris_snn(int,int,int32_t*,int8_t*,int8_t*,uint32_t*,int*);
int main(){
    int mn=127; for(int i=0;i<600;i++) if(iris_x_q8[i]<mn) mn=iris_x_q8[i];
    printf("min xq=%d (zp=-17 -> negative xi clamped if < -17)\n",mn);
    // per-sample compare: MLP logits vs SNN counts for first misclassified samples
    snn_image_t im; snn_build_iris(&im,256);
    for(int s=0,shown=0;s<150&&shown<4;s++){
        int32_t h[8]; int8_t hq[8];
        for(int j=0;j<8;j++){int32_t a=iris_b1[j];for(int k=0;k<4;k++)a+=(int32_t)iris_w1[4*j+k]*((int32_t)iris_x_q8[4*s+k]+17);
            int8_t v=rq(a,iris_m1_m0[j],iris_m1_sh[j],-128); hq[j]=v<-128?-128:v; if(hq[j]<-128)hq[j]=-128; h[j]=a;}
        int8_t oq[3];
        for(int o=0;o<3;o++){int32_t a=iris_b2[o];for(int j=0;j<8;j++)a+=(int32_t)iris_w2[8*o+j]*((int32_t)hq[j]+128);
            oq[o]=rq(a,iris_m2_m0[o],iris_m2_sh[o],24);}
        // run SNN on just this sample
        int8_t x1[4]; int8_t y1[1]={iris_y[s]};
        for(int k=0;k<4;k++) x1[k]=iris_x_q8[4*s+k];
        uint32_t cs; int acc;
        static int8_t xb[SNN_XMEM_B], yb[SNN_YMEM_B];
        for(int k=0;k<4;k++) xb[k]=x1[k]; yb[0]=y1[0];
        // hack: n_samp=1
        iris_snn(1,1,im.cfg,xb,yb,&cs,&acc);
        int mb=0; for(int o=1;o<3;o++) if(oq[o]>oq[mb]) mb=o;
        if(mb!=iris_y[s]||acc==0){
            printf("s=%3d y=%d mlp:oq-zp=[%d %d %d](best %d) snn acc=%d\n",
                   s,iris_y[s],oq[0]-24,oq[1]-24,oq[2]-24,mb,acc);
            shown++;
        }
    }
    return 0;
}
