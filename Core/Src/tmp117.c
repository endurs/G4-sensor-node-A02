#include "tmp117.h"
#include <string.h>

static tmp117_t volatile *g_active_dev = NULL;

void TMP117_Init(tmp117_t *dev, I2C_HandleTypeDef *hi2c, uint8_t addr7)
{
    memset(dev, 0, sizeof(*dev));
    dev->hi2c     = hi2c;
    dev->dev_addr = TMP117_HAL_ADDR(addr7);
}

HAL_StatusTypeDef TMP117_IsReady(tmp117_t *dev, uint32_t trials)
{
    return HAL_I2C_IsDeviceReady(dev->hi2c, dev->dev_addr, trials, HAL_MAX_DELAY);
}

HAL_StatusTypeDef TMP117_ReadDeviceID(tmp117_t *dev, uint16_t *out_id)
{
    uint8_t be[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->dev_addr,
                                            TMP117_REG_DEVICE_ID, I2C_MEMADD_SIZE_8BIT,
                                            be, 2, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    *out_id = (uint16_t)((be[0] << 8) | be[1]);
    return HAL_OK;
}

HAL_StatusTypeDef TMP117_ReadTemp_Blocking(tmp117_t *dev, float *degC)
{
    uint8_t be[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->dev_addr,
                                            TMP117_REG_TEMP, I2C_MEMADD_SIZE_8BIT,
                                            be, 2, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    int16_t raw = (int16_t)((be[0] << 8) | be[1]);
    if (degC) *degC = TMP117_RawToC(raw);
    dev->last_raw  = raw;
    dev->last_degC = TMP117_RawToC(raw);
    dev->new_data  = 1u;
    return HAL_OK;
}

HAL_StatusTypeDef TMP117_StartReadTemp_DMA(tmp117_t *dev)
{
    if (dev->busy || g_active_dev) return HAL_BUSY;

    dev->busy = 1u;
    g_active_dev = dev;
    dev->pending = 0u; // we are about to service it

    HAL_StatusTypeDef st = HAL_I2C_Mem_Read_DMA(dev->hi2c, dev->dev_addr,
                                                TMP117_REG_TEMP, I2C_MEMADD_SIZE_8BIT,
                                                dev->rx_buf, 2);
    if (st != HAL_OK) {
        dev->busy = 0u;
        g_active_dev = NULL;
    }
    return st;
}

void TMP117_I2C_MemRxCpltIRQ(I2C_HandleTypeDef *hi2c)
{
    if (g_active_dev && g_active_dev->hi2c == hi2c) {
        int16_t raw = (int16_t)((g_active_dev->rx_buf[0] << 8) | g_active_dev->rx_buf[1]);
        g_active_dev->last_raw  = raw;
        g_active_dev->last_degC = TMP117_RawToC(raw);
        g_active_dev->new_data  = 1u;
        g_active_dev->busy      = 0u;
        g_active_dev            = NULL;
    }
}

HAL_StatusTypeDef TMP117_ReadConfig(tmp117_t *dev, uint16_t *cfg)
{
    uint8_t be[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(dev->hi2c, dev->dev_addr,
                                            TMP117_REG_CONFIG, I2C_MEMADD_SIZE_8BIT,
                                            be, 2, HAL_MAX_DELAY);
    if (st != HAL_OK) return st;
    *cfg = (uint16_t)((be[0] << 8) | be[1]);
    return HAL_OK;
}

HAL_StatusTypeDef TMP117_WriteConfig(tmp117_t *dev, uint16_t cfg)
{
    uint8_t be[2] = { (uint8_t)(cfg >> 8), (uint8_t)(cfg & 0xFF) };
    return HAL_I2C_Mem_Write(dev->hi2c, dev->dev_addr,
                             TMP117_REG_CONFIG, I2C_MEMADD_SIZE_8BIT,
                             be, 2, HAL_MAX_DELAY);
}

HAL_StatusTypeDef TMP117_EnableDRDYonAlert(tmp117_t *dev, uint8_t active_high)
{
    uint16_t cfg;
    HAL_StatusTypeDef st = TMP117_ReadConfig(dev, &cfg);
    if (st != HAL_OK) return st;

    cfg |= TMP117_CFG_DRDY_ON_ALERT;           // DRDY on ALERT
    cfg &= ~((uint16_t)(3u << TMP117_CFG_MOD1_Pos)); // ensure MOD = 00 (continuous)
    if (active_high) cfg |= TMP117_CFG_ALERT_ACTIVE_HIGH;
    else             cfg &= ~TMP117_CFG_ALERT_ACTIVE_HIGH;

    return TMP117_WriteConfig(dev, cfg);
}

// Simple scheduler: prioritize devices with pending=1
int TMP117_Scheduler_Service(tmp117_t **dev_list, uint8_t dev_count)
{
    if (g_active_dev) return 0;

    for (uint8_t i = 0; i < dev_count; i++) {
        tmp117_t *d = dev_list[i];
        if (d->pending && !d->busy){
            if (TMP117_StartReadTemp_DMA(d) == HAL_OK) return 1;
        }
    }
    return 0;
}
