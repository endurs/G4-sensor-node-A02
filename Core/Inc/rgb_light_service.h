#ifndef RGB_LIGHT_SERVICE
#define RGB_LIGHT_SERVICE

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RGB_MODE_OFF = 0,
    RGB_MODE_RAINBOW,
    RGB_MODE_TEMPERATURE,
    RGB_MODE_HSI_SOLID
} RgbMode;

void Rgb_Light_Service_Init(void);
void Rgb_Light_Service_Update(void);

void Rgb_Light_Service_SetMode(RgbMode mode);

void Rgb_Light_Service_SetHSI(float h, float s, float i);
void Rgb_Light_Service_SetTemperatures(const float *temps4);


#ifdef __cplusplus
}
#endif
#endif
