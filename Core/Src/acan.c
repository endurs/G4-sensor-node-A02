#include "acan.h"
#include <string.h>

static FDCAN_HandleTypeDef *s_hfdcan = NULL;

static volatile bool    s_hasNewFrame = false;
static TelemetryFrame_t s_lastFrame;


static void ACAN_FillTxHeader(FDCAN_TxHeaderTypeDef *pHeader)
{
    pHeader->Identifier          = ACAN_STDID_TELEMETRY;
    pHeader->IdType              = FDCAN_STANDARD_ID;
    pHeader->TxFrameType         = FDCAN_DATA_FRAME;
    pHeader->FDFormat            = FDCAN_FD_CAN;
    pHeader->BitRateSwitch       = FDCAN_BRS_ON;
    pHeader->ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    pHeader->TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    pHeader->MessageMarker       = 0;
    pHeader->DataLength          = FDCAN_DLC_BYTES_48;
}


void ACAN_Init(FDCAN_HandleTypeDef *hfdcan)
{
    s_hfdcan = hfdcan;

    FDCAN_FilterTypeDef sFilterConfig = {0};
    sFilterConfig.IdType       = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex  = 0U;
    sFilterConfig.FilterType   = FDCAN_FILTER_RANGE_NO_EIDM;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1    = 0x000U;
    sFilterConfig.FilterID2    = 0x7FFU;
    HAL_StatusTypeDef st;

    st = HAL_FDCAN_ConfigFilter(s_hfdcan, &sFilterConfig);
    if (st != HAL_OK) {
        // breakpoint here, inspect s_hfdcan->ErrorCode
    }

    st = HAL_FDCAN_Start(s_hfdcan);
    if (st != HAL_OK) {
        // breakpoint here, inspect ErrorCode and Instance->PSR / CCCR
    }

    st = HAL_FDCAN_ActivateNotification(s_hfdcan,
                                        FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                        0U);
    if (st != HAL_OK) {
        // breakpoint
    }
}

HAL_StatusTypeDef ACAN_Send(const TelemetryFrame_t *frame)
{
    if (frame == NULL) {
        return HAL_ERROR;
    }

    if (s_hfdcan == NULL) {
        // Means ACAN_Init() was never called with a valid handle
        return HAL_ERROR;
    }

    FDCAN_TxHeaderTypeDef txHeader;
    ACAN_FillTxHeader(&txHeader);

    HAL_StatusTypeDef st = HAL_FDCAN_AddMessageToTxFifoQ(s_hfdcan, &txHeader,
                                                         (uint8_t const *)frame);
    if (st != HAL_OK) {
        uint32_t err = HAL_FDCAN_GetError(s_hfdcan);    // or just read s_hfdcan->ErrorCode
        // Put a breakpoint here and inspect:
        //   err / s_hfdcan->ErrorCode
        // and maybe s_hfdcan->State
    }

    return st;
}



bool ACAN_HasNewFrame(void)
{
    return s_hasNewFrame;
}


bool ACAN_GetLastFrame(TelemetryFrame_t *out)
{
    if ((out == NULL) || !s_hasNewFrame) {
        return false;
    }

    __disable_irq();
    memcpy(out, (const void *)&s_lastFrame, sizeof(TelemetryFrame_t));
    s_hasNewFrame = false;
    __enable_irq();

    return true;
}


/* ------------------------------------------------------------------------- */
/*  HAL FDCAN callback                                                       */
/*  This overrides the weak definition in stm32g4xx_hal_fdcan.c.             */
/*  Make sure you only implement this once in your project.                  */
/* ------------------------------------------------------------------------- */

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((hfdcan != s_hfdcan) ||
        ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U)) {
        return;
    }

    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t               rxData[64];

    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0,
                               &rxHeader, rxData) != HAL_OK) {
        return;
    }

    /* Only store frames that match our expected payload size */
    if (rxHeader.DataLength == FDCAN_DLC_BYTES_48) {
        memcpy((void *)&s_lastFrame, rxData, sizeof(TelemetryFrame_t));
        s_hasNewFrame = true;
    }
}