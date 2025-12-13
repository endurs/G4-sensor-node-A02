#ifndef RGB_LIGHT_SERVICE
#define RGB_LIGHT_SERVICE

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RGB_DISPLAY_OFF = 0,
    RGB_DISPLAY_RAINBOW,
    RGB_DISPLAY_TEMPERATURE,
    RGB_DISPLAY_HSI_SOLID
} RgbDisplayState;

void rgb_light_service_init(void);
void rgb_light_service_update(void);

void rgb_light_service_set_state(RgbDisplayState state);

void rgb_light_service_set_hsi(float h, float s, float i);
void rgb_light_service_set_temperatures(const float *temps4);


#ifdef __cplusplus
}
#endif
#endif
