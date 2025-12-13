#include "light_service.h"
#include "tim.h"
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define NUM_LED 4u
#define UPDATE_RATE 100u //Hz, this doesnt control actual update rate, it should instead reflect the set update rate from timer speed
#define ERROR_BLINK_RATE 2u
#define CYCLE_RATE 10u

static volatile uint16_t current_can_id = 0x0;
static volatile LedDIsplayState led_display_state = LED_DISPLAY_CANID;

//defining ports and pins for the four LEDs present
static GPIO_TypeDef* const  led_ports[] = {GPIOA,       GPIOB,      GPIOB,          GPIOB};
static const uint16_t       led_pins[]  = {GPIO_PIN_10, GPIO_PIN_9, GPIO_PIN_11,    GPIO_PIN_10};

//used for cycle and error blinking shit
static uint16_t cycle_count = 0u;
static uint16_t cycle_state = 0u;
static bool     cycle_dir   = true;



void light_service_init(uint16_t can_id){
      current_can_id = can_id;
}

void light_service_set_state(LedDIsplayState state){
    led_display_state = state;
}

void light_service_set_can_id(uint16_t can_id){
    current_can_id = can_id;
}


static void set_leds(const bool led_on[]){
    for (uint16_t i = 0; i< NUM_LED; i++){
        HAL_GPIO_WritePin(  led_ports[i], 
                         led_pins[i], 
                         led_on[i] ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}


static void led_display_can_id(uint16_t id){
    bool led_on[NUM_LED] = {false};
    for (uint16_t i = 0u; i<NUM_LED; i++){
        led_on[i] = ((id>>i) & 0x1u) != 0u;
    }

    set_leds(led_on);
}


static void led_display_cycle(uint16_t cycle_speed){
    bool led_on[NUM_LED] = {false};

    if (cycle_count >= (uint16_t)(UPDATE_RATE / cycle_speed) ){
        cycle_count = 0u;
        cycle_state++;
        if (cycle_state >= NUM_LED){
            cycle_state = 0u;
        }
    }
    
    led_on[cycle_state] = true;
    set_leds(led_on);
    cycle_count++;
}

static void led_display_bounce_cycle(uint16_t cycle_speed)
{
    bool led_on[NUM_LED] = {false};

    if (cycle_count >= (uint16_t)(UPDATE_RATE / cycle_speed)) {
        cycle_count = 0u;

        if (cycle_dir && cycle_state == NUM_LED - 1) {
            cycle_dir = false;
        } else if (!cycle_dir && cycle_state == 0) {
            cycle_dir = true;
        }

        if (cycle_dir) {
            cycle_state++;
        } else {
            cycle_state--;
        }
    }

    led_on[cycle_state] = true;
    set_leds(led_on);

    cycle_count++;
}


static void led_display_error(uint16_t cycle_speed){
    bool led_on[NUM_LED] = {false};
    
    if (cycle_speed == 0u) {
        cycle_speed = 1;
    }

    if (cycle_count >= (uint16_t)(UPDATE_RATE / cycle_speed) ){
        cycle_count = 0u;
        cycle_state++;
        if (cycle_state >= 2){
            cycle_state = 0u;
        }
    }

    if (cycle_state == 0) {
        for (uint16_t i = 0u; i<NUM_LED; i++){
            led_on[i] = true;
        }
    }
    
    set_leds(led_on);
    cycle_count++;
}

void light_service_update(void){

    //LED ARRAY
    switch (led_display_state) {
        case LED_DISPLAY_CANID:
            led_display_can_id(current_can_id);
            break;

        case LED_DISPLAY_CYCLE:
            led_display_cycle(CYCLE_RATE);
            break;

        case LED_DISPLAY_BOUNCE_CYCLE:
            led_display_bounce_cycle(CYCLE_RATE);
            break;

        case LED_DISPLAY_ERROR:
            led_display_error(CYCLE_RATE);
            break;

        case LED_DISPLAY_OFF:
            bool led_on[NUM_LED] = {false};
            set_leds(led_on);
            break;

        default:
            break;
    }
}
