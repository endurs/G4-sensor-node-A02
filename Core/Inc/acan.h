#ifndef ACAN_H
#define ACAN_H 

#include "fdcan.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ACAN_STDID_TELEMETRY (0x123)

typedef struct __attribute__((packed)) {
    double  timestamp;  //8 bytes
    float   value[8];   //8*4 bytes = 40 bytes
    uint8_t   reserved[8]; //padding to get 48 bytes
}TelemetryFrame_t;

_Static_assert( sizeof(TelemetryFrame_t) == 48U,
                "TelemetryFrame_t must be exactly 48 bytes");



/*
* initialize a ACAN
* configures a filer
* starts pheriphiral and rx interrupt
*/
void ACAN_Init(FDCAN_HandleTypeDef *hfdcan);

HAL_StatusTypeDef ACAN_Send(const TelemetryFrame_t *frame);

bool ACAN_HasNewFrame(void);

bool ACAN_GetLastFrame(TelemetryFrame_t *out);


#ifdef __cplusplus
}
#endif
#endif