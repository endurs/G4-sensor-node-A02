#ifndef DATA_APP
#define DATA_APP

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

void init_data_app(void);

void data_update_cjc_temp(float cj1, float cj2);

void data_process_adc_data(uint32_t timestamp, float adc_sample[4][2]);


#ifdef __cplusplus
}
#endif
#endif
