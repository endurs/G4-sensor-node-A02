#ifndef SK6812_H
#define SK6812_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * Minimal, DMA-driven SK6812/WS2812 driver using TIM5 CH1.
 * Configure in CubeMX:
 *   - TIM5 PWM CH1, PSC=0, ARR=212, OC preload ENABLED, polarity HIGH
 *   - DMA request: TIM5_CH1, Mem->Periph, Normal, High prio,
 *       Peripheral increment OFF, Memory increment ON,
 *       Data width Half-word (or Word if you prefer)
 *   - PB2 as AF for TIM5_CH1, Very High speed, no pull
 *
 * Electrical:
 *   - 5 V strip → use AHCT buffer for level shift, 150–330 Ω series R at DIN,
 *     ≥100 µF bulk cap on 5 V near first LED.
 */

/*** User-visible configuration (can be overridden with -D or before #include) ***/
#ifndef LED_COUNT
#define LED_COUNT        4u            // length of your strip
#endif

/* Uncomment if your LEDs are SK6812-RGBW; default is 3-byte RGB/GRB. */
/* #define LED_RGBW */

#ifndef LED_GRB_ORDER
#define LED_GRB_ORDER                   // SK6812/WS2812 usually expect GRB
#endif
/*** End of configuration ***/

void    sk6812_init(void);                          // call once after MX_TIM5_Init()
void    sk6812_set_rgb(uint16_t i, uint8_t r, uint8_t g, uint8_t b);
void    sk6812_set_hsi(uint16_t idx, float h, float s, float i); // NEW: Set LED color by HSI
void    sk6812_set_temperature(uint16_t idx, float temp, float min, float max);

void   sk6812_show(void);                          // blocking send
bool   sk6812_busy(void);                          // true while DMA active

#ifdef __cplusplus
}
#endif
#endif /* SK6812_H */
