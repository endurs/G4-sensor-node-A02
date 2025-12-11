#ifndef LIGHT_SERVICE
#define LIGHT_SERVICE

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_DISPLAY_OFF = 0,
    LED_DISPLAY_CANID,
    LED_DISPLAY_CYCLE,
    LED_DISPLAY_BOUNCE_CYCLE,
    LED_DISPLAY_ERROR
}LedDIsplayState;

extern bool RUN_LIGHT_SERVICE;

void Light_Service_Init(uint16_t can_id);

void Light_Service_Update(void);

void Light_Service_Set_Display_State(LedDIsplayState);

void Light_Service_Set_Can_Id(uint16_t can_id);



#ifdef __cplusplus
}
#endif
#endif
