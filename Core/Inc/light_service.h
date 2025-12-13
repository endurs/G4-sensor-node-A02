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

void light_service_init(uint16_t can_id);

void light_service_update(void);

void light_service_set_state(LedDIsplayState);

void light_service_set_can_id(uint16_t can_id);



#ifdef __cplusplus
}
#endif
#endif
