#include "adc_dma.h"
#include "adc.h"
#include "tim.h"
#include <math.h>
#include <stdint.h>
#include "data_app.h"
#include "main.h" //for hal time now thing?


// DMA backing buffers, interleaved [R1,R2,R1,R2,...]
static uint16_t adc1_buf[ADC_DMA_BUF_LEN];
static uint16_t adc2_buf[ADC_DMA_BUF_LEN];
static uint16_t adc3_buf[ADC_DMA_BUF_LEN];
static uint16_t adc4_buf[ADC_DMA_BUF_LEN];

static float adc_sample[4][2] = {0}; //in voltage
static uint32_t s_sample_index = 0;

static volatile uint8_t s_adc_frame_ready_mask = 0u;
static const float adc_q =  3.3f / 65535.0f; //assuming 3.3V and 16bit adc


static inline int idx_from_adc(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) return 0;
    if (hadc->Instance == ADC2) return 1;
    if (hadc->Instance == ADC3) return 2;
    if (hadc->Instance == ADC4) return 3;
    return -1;
}

static inline uint16_t* buf_from_adc(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) return adc1_buf;
    if (hadc->Instance == ADC2) return adc2_buf;
    if (hadc->Instance == ADC3) return adc3_buf;
    if (hadc->Instance == ADC4) return adc4_buf;
    return NULL;
}

// Process one half of a DMA buffer
static void process_half_block(ADC_HandleTypeDef *hadc,
                               uint16_t *base,
                               uint32_t offset_words)
{
    uint32_t sum_r1 = 0, sum_r2 = 0;

    uint32_t start = offset_words;
    uint32_t end   = offset_words + ADC_WORDS_PER_HALF;

    for (uint32_t i = start; i < end; i += 2) {
        sum_r1 += base[i + 0];  // rank 0
        sum_r2 += base[i + 1];  // rank 1
    }

    int k = idx_from_adc(hadc);
    if (k < 0) {
        return;
    }

    // Decimation/averaging (DECIM_FACTOR=1 means just the single sample)
    float avg_r1 = (float)sum_r1 / (float)ADC_DECIM_FACTOR;
    float avg_r2 = (float)sum_r2 / (float)ADC_DECIM_FACTOR;

    adc_sample[k][0] = avg_r1 * adc_q; // TC voltage
    adc_sample[k][1] = avg_r2 * adc_q; // PT voltage

    // Mark this ADC as updated for this TRGO
    s_adc_frame_ready_mask |= (1u << k);

    // When all 4 ADCs have updated for this sample, build and send frame
    if (s_adc_frame_ready_mask == 0x0Fu) {
        s_adc_frame_ready_mask = 0u;
        uint32_t sample_index = s_sample_index++;
        data_process_adc_data(sample_index, adc_sample);
    }
}


void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    uint16_t *buf = buf_from_adc(hadc);
    if (buf) process_half_block(hadc, buf, 0u);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    uint16_t *buf = buf_from_adc(hadc);
    if (buf) process_half_block(hadc, buf, ADC_WORDS_PER_HALF);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    (void)hadc;
}

// --- public start-up ------------------------------------------------------

void ADC_DMA_StartAll(void) {
    // 1) Calibrate each ADC in single-ended mode (required after reset)
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);

    // 2) Start DMA streams (circular). Length is in halfwords.
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc4, (uint32_t*)adc4_buf, ADC_DMA_BUF_LEN);

    // 3) Start the TRGO timer last, so everyone is armed before first trigger
    HAL_TIM_Base_Start(&htim6);
}
