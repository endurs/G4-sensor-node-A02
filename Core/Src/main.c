/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fdcan.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "math.h"
#include "sk6812.h"
#include "tmp117.h"
#include "adc_dma.h"
#include "tc_k.h"
#include "acan.h"
#include "light_service.h"
#include "rgb_light_service.h"
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c4;   // from i2c.c

extern FDCAN_HandleTypeDef hfdcan3;

// Driver instances
static tmp117_t g_tmp117_48;  // ALERT on PC4, addr 0x48
static tmp117_t g_tmp117_49;  // ALERT on PB0, addr 0x49

static tmp117_t *g_tmp_list[] = {
    &g_tmp117_48,
    &g_tmp117_49
};

static const uint8_t g_tmp_count = sizeof(g_tmp_list) / sizeof(g_tmp_list[0]);

//TC stuff
static tc_k_ctx_t g_tc_k;
volatile float g_tc_degC[4] = {0};

const uint32_t adc_buffer_read_delay_ms = 100; // Delay between ADC DMA reads

const uint32_t acan_msg_freq = 2; //hz

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM5_Init();
  MX_I2C4_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_TIM6_Init();
  MX_FDCAN3_Init();
  MX_RTC_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
  

  //ADC and thermocouple
  tc_k_init(&g_tc_k, (tc_k_cfg_t) {
    .gain = 110.89f, 
    .v_offset = 1.235f
    });
        
  ADC_DMA_StartAll();
  

  //on-board temp sensors
  TMP117_Init(&g_tmp117_49, &hi2c4, 0x49);
  TMP117_Init(&g_tmp117_48, &hi2c4, 0x48);

  TMP117_EnableDRDYonAlert(&g_tmp117_49, 0);
  TMP117_EnableDRDYonAlert(&g_tmp117_48, 0);


  //CAN bus
  ACAN_Init(&hfdcan3);

  //LED services
  light_service_init(0x5);
  light_service_set_state(LED_DISPLAY_BOUNCE_CYCLE);
  
  rgb_light_service_init();
  rgb_light_service_set_state(RGB_DISPLAY_RAINBOW);
  
  HAL_TIM_Base_Start_IT(&htim7); //interrupt running rgb and light service updates


  uint32_t last_adc_poll = HAL_GetTick();

  volatile float PT1_v = 0;
  volatile float PT2_v = 0;
  volatile float PT3_v = 0;
  volatile float PT4_v = 0;

  volatile float TC1_v = 0;
  volatile float TC2_v = 0;
  volatile float TC3_v = 0;
  volatile float TC4_v = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */


  while (1)
  {
    uint32_t now = HAL_GetTick();

    if (now - last_adc_poll >= adc_buffer_read_delay_ms) {
      last_adc_poll = now;

      const float q = 3.3f / 65535.0f; //65535.0f;

      PT1_v = g_adc_avg_500Hz[0][1] * q;
      PT2_v = g_adc_avg_500Hz[1][1] * q;
      PT3_v = g_adc_avg_500Hz[2][1] * q;
      PT4_v = g_adc_avg_500Hz[3][1] * q;

      TC1_v = g_adc_avg_500Hz[0][0] * q;
      TC2_v = g_adc_avg_500Hz[1][0] * q;
      TC3_v = g_adc_avg_500Hz[2][0] * q;
      TC4_v = g_adc_avg_500Hz[3][0] * q;


      TelemetryFrame_t can_msg = {  .timestamp = now,
                                    .value = {PT1_v, PT2_v, PT3_v, PT4_v, g_tc_degC[0], g_tc_degC[1], g_tc_degC[2], g_tc_degC[3]},
                                    .reserved = {0}
      };

      ACAN_Send(&can_msg);
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */


  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV6;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV8;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  TMP117_I2C_MemRxCpltIRQ(hi2c);

  //todo put cjc pusher function in here

  TMP117_Scheduler_Service(g_tmp_list, g_tmp_count);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7) {
        light_service_update();
        rgb_light_service_update();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0) {
        // ALERT from TMP117 at 0x49 (PB0)
        TMP117_MarkPending(&g_tmp117_49);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        // ALERT from TMP117 at 0x48 (PC4)
        TMP117_MarkPending(&g_tmp117_48);
    } else {
        return;
    }

    TMP117_Scheduler_Service(g_tmp_list, g_tmp_count);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
