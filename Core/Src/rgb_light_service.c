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

static RgbMode current_mode = RGB_MODE_OFF;

static float current_h = 0.0f;
static float current_s = 1.0f;
static float current_i = 1.0f;

static float current_temps[LED_COUNT] = {0};
static float rainbow_hue = 0.0f;

static uint16_t cycle_count = 0u;
static uint16_t cycle_state = 0u;

//halpers
static void Rgb_Display_Rainbow(void);
static void Rgb_Display_Temperature(void);
static void Rgb_Display_HSI_Solid(void);

void Rgb_Light_Service_Init(void){

    sk6812_init();
    
    for (uint16_t i = 0u; i < LED_COUNT; i++){
        current_temps[i] = OC_TC_TEMP+1;
    }
}

void Rgb_Light_Service_Update(void)
{
    switch (current_mode) {
    case RGB_MODE_OFF:
        for (uint16_t i = 0; i < LED_COUNT; ++i) {
            sk6812_set_rgb(i, 0, 0, 0);
        }
        break;

    case RGB_MODE_RAINBOW:
        Rgb_Display_Rainbow();
        break;

    case RGB_MODE_TEMPERATURE:
        Rgb_Display_Temperature();
        break;

    case RGB_MODE_HSI_SOLID:
        Rgb_Display_HSI_Solid();
        break;

    default:
        break;
    }
    sk6812_show();
}

void Rgb_Light_Service_SetMode(RgbMode mode){
    current_mode = mode;
}

void Rgb_Light_Service_SetTemperatures(const float *temps4)
{
    for (int i = 0; i < 4; ++i) {
        current_temps[i] = temps4[i];
    }
}

void Rgb_Light_Service_SetHSI(float h, float s, float i)
{
    current_h = h;
    current_s = s;
    current_i = i;
}

static void Rgb_Display_HSI_Solid(void)
{
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        sk6812_set_hsi(i, current_h, current_s, current_i);
    }
}

static void Rgb_Display_Rainbow(void)
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

static void Rgb_Display_Temperature(void)
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

