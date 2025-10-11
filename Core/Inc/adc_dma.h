#pragma once
#include "main.h"

/* Decimate 5 kHz → 500 Hz (10 triggers), 2 ranks per ADC */
#define ADC_DECIM_FACTOR        10u
#define ADC_RANKS_PER_ADC       2u
#define ADC_WORDS_PER_HALF      (ADC_DECIM_FACTOR * ADC_RANKS_PER_ADC)  /* 20 */
#define ADC_DMA_BUF_LEN         (2u * ADC_WORDS_PER_HALF)               /* 40 */

/* 8 outputs @ 500 Hz: [adc index 0..3][rank 0..1] */
extern volatile uint16_t g_adc_avg_500Hz[4][2];

/* Call once after MX_* init */
void ADC_DMA_StartAll(void);
