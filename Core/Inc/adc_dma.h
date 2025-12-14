#pragma once
#include "main.h"

#define ADC_DECIM_FACTOR        10u
#define ADC_RANKS_PER_ADC       2u
#define ADC_WORDS_PER_HALF      (ADC_DECIM_FACTOR * ADC_RANKS_PER_ADC)
#define ADC_DMA_BUF_LEN         (2u * ADC_WORDS_PER_HALF)


#define ADC_RANK1_FILTER_CUTOFF 100u  //filter cutoff frequency in Hz
#define ADC_RANK2_FILTER_CUTOFF 1000u 
#define ADC_RANK1_DATA_PUSH_RATE 300u //how often to push data on canbus in Hz
#define ADC_RANK2_DATA_PUSH_RATE 2500u

/* [adc index 0..3][rank 0..1] */
// extern volatile uint16_t g_adc_sample[4][2];

/* Call once after MX_* init */
void ADC_DMA_StartAll(void);
