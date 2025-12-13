#include "rgb_light_service.h"
#include "sk6812.h"
#include <math.h>
#include <stdint.h>

#define OC_TC_TEMP 470 //Degrees Celcius
#define UPDATE_RATE 100u 
#define NO_TEMP_BLINK_RATE 2u
#define NO_TEMP_BLINK_PWR 255 //uint8_t range 0-255

#define MIN_TEMP -5
#define MAX_TEMP 105

static RgbDisplayState current_state = RGB_DISPLAY_OFF;

static float current_h = 0.0f;
static float current_s = 1.0f;
static float current_i = 1.0f;

static float current_temps[LED_COUNT] = {0};
static float rainbow_hue = 0.0f;

static uint16_t cycle_count = 0u;
static uint16_t cycle_state = 0u;

//halpers
static void rgb_display_rainbow(void);
static void rgb_display_temperature(void);
static void rgb_display_hsi_solid(void);

void rgb_light_service_init(void){

    sk6812_init();
    
    for (uint16_t i = 0u; i < LED_COUNT; i++){
        current_temps[i] = OC_TC_TEMP+1;
    }
}

void rgb_light_service_update(void)
{
    switch (current_state) {
    case RGB_DISPLAY_OFF:
        for (uint16_t i = 0; i < LED_COUNT; ++i) {
            sk6812_set_rgb(i, 0, 0, 0);
        }
        break;

    case RGB_DISPLAY_RAINBOW:
        rgb_display_rainbow();
        break;

    case RGB_DISPLAY_TEMPERATURE:
        rgb_display_temperature();
        break;

    case RGB_DISPLAY_HSI_SOLID:
        rgb_display_hsi_solid();
        break;

    default:
        break;
    }
    sk6812_show();
}

void rgb_light_service_set_state(RgbDisplayState state){
    current_state = state;
}

void rgb_light_service_set_temperatures(const float *temps4)
{
    for (int i = 0; i < 4; ++i) {
        current_temps[i] = temps4[i];
    }
}

void rgb_light_service_set_hsi(float h, float s, float i)
{
    current_h = h;
    current_s = s;
    current_i = i;
}

static void rgb_display_hsi_solid(void)
{
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        sk6812_set_hsi(i, current_h, current_s, current_i);
    }
}

static void rgb_display_rainbow(void)
{
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        float hue = fmodf(rainbow_hue + (90.0f / LED_COUNT) * i, 360.0f);
        sk6812_set_hsi(i, hue, 1.0f, 1.0f);
    }

    rainbow_hue += 1.0f;
    if (rainbow_hue >= 360.0f) {
        rainbow_hue -= 360.0f;
    }
}

static void rgb_display_temperature(void)
{
    if (cycle_count >= (uint16_t)(UPDATE_RATE / NO_TEMP_BLINK_RATE)) {
        cycle_count = 0;
        cycle_state = (cycle_state == 0) ? 1 : 0;
    }
    cycle_count++;

    for (uint16_t i = 0; i < 4; ++i)
    {
      if (current_temps[3-i] >= OC_TC_TEMP) // if open circuit detected, blink
      {
        if (cycle_state == 0) {
          sk6812_set_rgb(i, NO_TEMP_BLINK_PWR, NO_TEMP_BLINK_PWR, NO_TEMP_BLINK_PWR);
        } else {
          sk6812_set_rgb(i, 0, 0, 0);
        }
      }
      else {
        sk6812_set_temperature(i, current_temps[3-i], MIN_TEMP, MAX_TEMP);
      }
    }
}

