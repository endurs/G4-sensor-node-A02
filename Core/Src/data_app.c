#include "data_app.h"
#include "acan.h"
#include "tc_k.h"
#include <stdint.h>
#include "rgb_light_service.h"

static float cjc_temps[4] = {0};
static float tc_temps[4]  = {0};

static float adc_tc[4] = {0};
static float adc_pt[4] = {0};

static tc_k_ctx_t tc_k_ctx; 

void init_data_app(void){

     tc_k_init(&tc_k_ctx, (tc_k_cfg_t) {
    .gain = 110.89f, 
    .v_offset = 1.235f
    });

}

void data_update_cjc_temp(float cj1, float cj3){
    float slope = (cj1 - cj3) * 0.5f;
    float est_cj_2 = cj1 + slope * 1.0f;
    float est_cj_4 = cj1 + slope * 3.0f;

    cjc_temps[0] = cj1;
    cjc_temps[1] = est_cj_2; 
    cjc_temps[2] = cj3;
    cjc_temps[3] = est_cj_4;

    
}

void data_process_adc_data(uint32_t now, float adc_data[4][2]){

    // Rank 1 = PT, rank 0 = TC
    for (int i = 0; i < 4; ++i) {
        adc_pt[i] = adc_data[i][1];
        adc_tc[i] = adc_data[i][0];
    }

    tc_k_convert4(&tc_k_ctx, adc_tc, cjc_temps, tc_temps);
    
    TelemetryFrame_t can_msg = {    .timestamp = now,
                                    .value = {adc_pt[0], adc_pt[1], adc_pt[2], adc_pt[3], tc_temps[0], tc_temps[1], tc_temps[2], tc_temps[3]},
                                    .reserved = {0}
    };

    ACAN_Send(&can_msg);
    rgb_light_service_set_temperatures(tc_temps); //this is kinda hacky it should really be in the data_process_data since thats where we get new TC data, but it happens so often so just do it here, its not that important.
}


