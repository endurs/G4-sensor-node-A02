#include "sk6812.h"
#include "tim.h"                // brings in TIM handles from CubeMX
#include "stm32g4xx_hal.h"
#include <math.h>               // for powf(), cosf(), fmodf()

/* === Bind the driver to your timer/channel (TIM5 CH1 on PB2) === */
#define SK_TIM          (&htim5)
#define SK_TIM_CH       TIM_CHANNEL_1

/* ===== Timing model =======================================================
 * We run one DMA word per 1 bit-cell at ~800 kHz (ARR=212 @170 MHz → 1.253 us).
 * Duty for '0' ≈ 0.3 us (~24%), for '1' ≈ 0.6 us (~48%).
 * The exact tick counts are derived from the actual ARR at runtime.
 */

#define BYTES_PER_LED  3u


#define BITS_PER_LED     (8u * BYTES_PER_LED)
#define RESET_SLOTS      100u             // ≥80 us low; ~100 us margin at 1.25 us/slot
#define STREAM_SLOTS     (LED_COUNT * BITS_PER_LED)
#define DMA_BUF_LEN      (STREAM_SLOTS + RESET_SLOTS)

/* Match your DMA data width: Half-word in CubeMX → uint16_t here.
 * If you switch to Word in CubeMX, change this to uint32_t.
 */
typedef uint32_t dma_word_t;


static dma_word_t dmaBuf[DMA_BUF_LEN];
static uint8_t    pixels[LED_COUNT * BYTES_PER_LED];

static volatile bool dmaBusy = false;

static uint32_t period_ticks = 0;   // ARR+1
static uint32_t T0H_ticks    = 0;   // ~24% of period
static uint32_t T1H_ticks    = 0;   // ~48% of period

/* Encode one byte MSB-first into 8 CCR (duty) values */
static inline void encode_byte(uint8_t b, dma_word_t *dst)
{
    for (int i = 7; i >= 0; --i) {
        dst[7 - i] = (b & (1u << i)) ? (dma_word_t)T1H_ticks
                                     : (dma_word_t)T0H_ticks;
    }
}

void sk6812_init(void)
{
    /* Compute from actual timer settings (robust against ARR changes). */
    period_ticks = __HAL_TIM_GET_AUTORELOAD(SK_TIM) + 1u;

    /* 0.24 and 0.48 duty approximations of 0.3/0.6 us at 1.253 us/bit. */
    T0H_ticks = (period_ticks * 24u + 50u) / 100u;   // rounded
    T1H_ticks = (period_ticks * 48u + 50u) / 100u;

    /* Ensure output idles low. */
    HAL_TIM_PWM_Stop(SK_TIM, SK_TIM_CH);
    __HAL_TIM_SET_COMPARE(SK_TIM, SK_TIM_CH, 0);
}

void sk6812_set_rgb(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= LED_COUNT) return;
#ifdef LED_GRB_ORDER
    pixels[idx*BYTES_PER_LED + 0] = g;
    pixels[idx*BYTES_PER_LED + 1] = r;
    pixels[idx*BYTES_PER_LED + 2] = b;
#else
    pixels[idx*BYTES_PER_LED + 0] = r;
    pixels[idx*BYTES_PER_LED + 1] = g;
    pixels[idx*BYTES_PER_LED + 2] = b;
#endif
}

static uint8_t gamma_correct(uint8_t value)
{
    float gamma = 2.2f;
    return (uint8_t)(powf((float)value / 255.0f, gamma) * 255.0f + 0.5f);
}

static void hsi2rgb(float H, float S, float I, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float PI = 3.14159265f;
    float cos_h, cos_1047_h;
    H = fmodf(H, 360.0f); // cycle H around to 0-360 degrees
    H = PI * H / 180.0f;  // convert to radians
    S = fminf(fmaxf(S, 0.0f), 1.0f);
    I = fminf(fmaxf(I, 0.0f), 1.0f);

    if (H < 2.09439f) {
        cos_h = cosf(H);
        cos_1047_h = cosf(1.047196667f - H);
        *r = (uint8_t)(255.0f * I * (1.0f + S * cos_h / cos_1047_h) / 3.0f);
        *g = (uint8_t)(255.0f * I * (1.0f + S * (1.0f - cos_h / cos_1047_h)) / 3.0f);
        *b = (uint8_t)(255.0f * I * (1.0f - S) / 3.0f);
        
    } else if (H < 4.188787f) {
        H = H - 2.09439f;
        cos_h = cosf(H);
        cos_1047_h = cosf(1.047196667f - H);
        *g = (uint8_t)(255.0f * I * (1.0f + S * cos_h / cos_1047_h) / 3.0f);
        *b = (uint8_t)(255.0f * I * (1.0f + S * (1.0f - cos_h / cos_1047_h)) / 3.0f);
        *r = (uint8_t)(255.0f * I * (1.0f - S) / 3.0f);
    } else {
        H = H - 4.188787f;
        cos_h = cosf(H);
        cos_1047_h = cosf(1.047196667f - H);
        *b = (uint8_t)(255.0f * I * (1.0f + S * cos_h / cos_1047_h) / 3.0f);
        *r = (uint8_t)(255.0f * I * (1.0f + S * (1.0f - cos_h / cos_1047_h)) / 3.0f);
        *g = (uint8_t)(255.0f * I * (1.0f - S) / 3.0f);
    }

    *r = gamma_correct(*r);
    *g = gamma_correct(*g);
    *b = gamma_correct(*b);
}

void sk6812_set_hsi(uint16_t idx, float h, float s, float i)
{
    uint8_t r, g, b;
    hsi2rgb(h, s, i, &r, &g, &b);
    sk6812_set_rgb(idx, r, g, b);
}

/**
 * Maps a temperature value to a color gradient from ice blue (cold) to red (hot).
 * @param idx   LED index
 * @param temp  Current temperature
 * @param min   Minimum temperature (coldest)
 * @param max   Maximum temperature (hottest)
 */
void sk6812_set_temperature(uint16_t idx, float temp, float min, float max)
{
    // Clamp temperature to [min, max]
    if (temp < min) temp = min;
    if (temp > max) temp = max;

    // Normalize temperature to [0, 1]
    float t = (temp - min) / (max - min);

    // Hue for ice blue ≈ 200°, red ≈ 0°
    float cold_hue = 200.0f;
    float hot_hue  = 0.0f;
    float hue = hot_hue + (cold_hue - hot_hue) * (1.0f - t);

    // Saturation and intensity can be fixed or mapped if desired
    float sat = 1.0f;
    float intensity = 1.0f;

    sk6812_set_hsi(idx, hue, sat, intensity);
}

bool sk6812_busy(void)
{
    return dmaBusy;
}

void sk6812_show(void)
{
    /* If you prefer non-blocking, just return when busy. */
    while (dmaBusy) { __NOP(); }

    /* 1) Encode pixel buffer into duty-cycle words (CCR values). */
    uint32_t w = 0;
    for (uint32_t i = 0; i < LED_COUNT; ++i) {
        for (uint32_t j = 0; j < BYTES_PER_LED; ++j) {
            encode_byte(pixels[i*BYTES_PER_LED + j], &dmaBuf[w]);
            w += 8;
        }
    }
    /* 2) Reset (latch) time: keep low by writing CCR=0 for a bunch of slots. */
    for (uint32_t r = 0; r < RESET_SLOTS; ++r) {
        dmaBuf[w++] = 0;
    }

    /* 3) Kick PWM + DMA. CubeMX must have OC preload enabled. */
    dmaBusy = true;
    __HAL_TIM_SET_AUTORELOAD(SK_TIM, (uint32_t)(period_ticks - 1u));
    __HAL_TIM_SET_COUNTER(SK_TIM, 0);
    HAL_TIM_PWM_Start_DMA(SK_TIM, SK_TIM_CH, (uint32_t*)dmaBuf, w);

    /* MVP behavior: block until DMA finishes. */
    while (dmaBusy) { __NOP(); }
}

/* HAL invokes this when the DMA transfer for the PWM channel completes. */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5) {
        HAL_TIM_PWM_Stop_DMA(SK_TIM, SK_TIM_CH);
        __HAL_TIM_SET_COMPARE(SK_TIM, SK_TIM_CH, 0);   // idle low
        dmaBusy = false;
    }
}
