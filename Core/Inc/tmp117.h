#ifndef TMP117_H
#define TMP117_H

#include "stm32g4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- TMP117 register map ---
#define TMP117_REG_TEMP        0x00u
#define TMP117_REG_CONFIG      0x01u
#define TMP117_REG_T_HIGH      0x02u
#define TMP117_REG_T_LOW       0x03u
#define TMP117_REG_EEPROM_UL   0x04u
#define TMP117_REG_EEPROM1     0x05u
#define TMP117_REG_EEPROM2     0x06u
#define TMP117_REG_TEMP_OFFS   0x07u
#define TMP117_REG_EEPROM3     0x08u
#define TMP117_REG_DEVICE_ID   0x0Fu  // expected 0x0117

// --- CONFIG bits (Table 7-6 TI DS) ---
#define TMP117_CFG_MOD1_Pos    11u
#define TMP117_CFG_MOD0_Pos    10u
#define TMP117_CFG_CONV2_Pos    9u
#define TMP117_CFG_CONV1_Pos    8u
#define TMP117_CFG_CONV0_Pos    7u
#define TMP117_CFG_AVG1_Pos     6u
#define TMP117_CFG_AVG0_Pos     5u
#define TMP117_CFG_TnA_Pos      4u
#define TMP117_CFG_POL_Pos      3u
#define TMP117_CFG_DRALERT_Pos  2u
#define TMP117_CFG_SRST_Pos     1u

#define TMP117_CFG_CONTINUOUS   (0u << TMP117_CFG_MOD1_Pos) // MOD=00
#define TMP117_CFG_DRDY_ON_ALERT (1u << TMP117_CFG_DRALERT_Pos)
#define TMP117_CFG_ALERT_ACTIVE_HIGH (1u << TMP117_CFG_POL_Pos)

// LSB = 0.0078125 C
#define TMP117_LSB_C           (0.0078125f)

// HAL uses 8-bit "addr << 1"
#define TMP117_HAL_ADDR(addr7) ((uint16_t)((addr7) << 1))

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t           dev_addr;     // 8-bit HAL address
    uint8_t            rx_buf[2];    // DMA target
    volatile uint8_t   busy;         // transfer in flight
    volatile uint8_t   new_data;     // set on DMA complete
    volatile uint8_t   pending;      // DRDY asserted (EXTI) -> schedule read
    int16_t            last_raw;
    float              last_degC;
} tmp117_t;

// --- API ---
void TMP117_Init(tmp117_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr7);

HAL_StatusTypeDef TMP117_IsReady(tmp117_t *dev, uint32_t trials);
HAL_StatusTypeDef TMP117_ReadDeviceID(tmp117_t *dev, uint16_t *out_id);

HAL_StatusTypeDef TMP117_ReadTemp_Blocking(tmp117_t *dev, float *degC);
HAL_StatusTypeDef TMP117_StartReadTemp_DMA(tmp117_t *dev);

// DRDY helpers
static inline void TMP117_MarkPending(tmp117_t *dev) { dev->pending = 1u; }
// Try to start a DMA read for any pending device; returns 1 if started.
int TMP117_Scheduler_Service(tmp117_t **dev_list, uint8_t dev_count);

// IRQ fan-in from HAL
void TMP117_I2C_MemRxCpltIRQ(I2C_HandleTypeDef *hi2c);

// Config helpers
HAL_StatusTypeDef TMP117_ReadConfig(tmp117_t *dev, uint16_t *cfg);
HAL_StatusTypeDef TMP117_WriteConfig(tmp117_t *dev, uint16_t cfg);

// Enable DRDY on ALERT pin; active_high=0 for falling-edge EXTI (default).
HAL_StatusTypeDef TMP117_EnableDRDYonAlert(tmp117_t *dev, uint8_t active_high);

// Utility
static inline float TMP117_RawToC(int16_t raw) { return ((float)raw) * TMP117_LSB_C; }

#ifdef __cplusplus
}
#endif
#endif // TMP117_H
