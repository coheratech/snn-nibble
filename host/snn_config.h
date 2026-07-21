/* SNN conversion: builds runtime config (weights, per-step biases, thresholds from the
 * requant scales) from the shared quantized model. Host-side float is fine (config gen). */
#ifndef SNN_CONFIG_H
#define SNN_CONFIG_H
#include <string.h>
#include <math.h>
#include "iris_model.h"
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

typedef struct { int32_t cfg[SNN_CFG_W]; int8_t xmem[SNN_XMEM_B]; int8_t ymem[SNN_YMEM_B]; int n_samp; } snn_image_t;

static void snn_build_iris(snn_image_t *im, int T){
    memset(im,0,sizeof(*im));
    im->cfg[CFG_T]=T;
    for(int i=0;i<32;i++) im->cfg[CFG_W1+i]=iris_w1[i];
    for(int j=0;j<8;j++){
        im->cfg[CFG_B1+j]=iris_b1[j];
        double scale=(double)iris_m1_m0[j]/2147483648.0/(double)(1<<iris_m1_sh[j]);
        im->cfg[CFG_TH1+j]=(int32_t)llround(256.0/scale);
    }
    for(int i=0;i<24;i++) im->cfg[CFG_W2+i]=iris_w2[i];
    for(int o=0;o<3;o++){
        im->cfg[CFG_B2+o]=iris_b2[o];
        double scale=(double)iris_m2_m0[o]/2147483648.0/(double)(1<<iris_m2_sh[o]);
        im->cfg[CFG_TH2+o]=(int32_t)llround(256.0/scale);
    }
    memcpy(im->xmem,iris_x_q8,600); memcpy(im->ymem,iris_y,150); im->n_samp=150;
}
#endif
