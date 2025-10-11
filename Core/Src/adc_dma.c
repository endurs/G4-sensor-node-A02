#include "adc_dma.h"
#include "adc.h"
#include "tim.h"

/* DMA backing buffers, interleaved [R1,R2,R1,R2,...] */
static uint16_t adc1_buf[ADC_DMA_BUF_LEN];
static uint16_t adc2_buf[ADC_DMA_BUF_LEN];
static uint16_t adc3_buf[ADC_DMA_BUF_LEN];
static uint16_t adc4_buf[ADC_DMA_BUF_LEN];

/* Public: last computed 500 Hz boxcar averages (12-bit range) */
volatile uint16_t g_adc_avg_500Hz[4][2] = {0};

/* --- helpers -------------------------------------------------------------- */

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

/* Process one half of a DMA buffer: sum even/odd indices (R1/R2) */
static void process_half_block(ADC_HandleTypeDef *hadc, uint16_t *base, uint32_t offset_words) {
    uint32_t sum_r1 = 0, sum_r2 = 0;

    /* We know one half = 20 words = 10 triggers of [R1,R2] pairs */
    uint32_t start = offset_words;
    uint32_t end   = offset_words + ADC_WORDS_PER_HALF; /* exclusive */

    /* Sum even (rank1) and odd (rank2) positions */
    for (uint32_t i = start; i < end; i += 2) {
        sum_r1 += base[i + 0];
        sum_r2 += base[i + 1];
    }

    int k = idx_from_adc(hadc);
    if (k >= 0) {
        /* Boxcar average over 10 samples each */
        g_adc_avg_500Hz[k][0] = (uint16_t)((sum_r1 + ADC_DECIM_FACTOR/2) / ADC_DECIM_FACTOR);
        g_adc_avg_500Hz[k][1] = (uint16_t)((sum_r2 + ADC_DECIM_FACTOR/2) / ADC_DECIM_FACTOR);
    }
}

/* --- HAL hook-up (CubeMX will weak-link these) --------------------------- */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    uint16_t *buf = buf_from_adc(hadc);
    if (buf) process_half_block(hadc, buf, 0u);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    uint16_t *buf = buf_from_adc(hadc);
    if (buf) process_half_block(hadc, buf, ADC_WORDS_PER_HALF);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    /* Optional: breakpoint/log; with Overrun=Overwrite this should stay quiet */
    (void)hadc;
}

/* --- public start-up ------------------------------------------------------ */

void ADC_DMA_StartAll(void) {
    /* 1) Calibrate each ADC in single-ended mode (required after reset) */
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
    HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);

    /* 2) Start DMA streams (circular). Length is in halfwords. */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_buf, ADC_DMA_BUF_LEN);
    HAL_ADC_Start_DMA(&hadc4, (uint32_t*)adc4_buf, ADC_DMA_BUF_LEN);

    /* 3) Start the TRGO timer last, so everyone is armed before first trigger */
    HAL_TIM_Base_Start(&htim6);
}
