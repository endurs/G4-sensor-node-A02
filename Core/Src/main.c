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
static tmp117_t t_49; // ADD0=V+ (7-bit 0x49)
static tmp117_t t_48; // ADD0=GND (7-bit 0x48)


//TC stuff
static tc_k_ctx_t g_tc_k;
volatile float g_tc_degC[4] = {0};

// Easy-to-watch debug variables
volatile float   g_tmp117_c_49 = 0.0f;
volatile float   g_tmp117_c_48 = 0.0f;
volatile int16_t g_tmp117_raw_49 = 0;
volatile int16_t g_tmp117_raw_48 = 0;

// Periodic sampling setup (100 ms is conservative and well above conv. time)
static const uint32_t TMP117_SAMPLE_PERIOD_MS = 10;
static uint32_t last_sample_49 = 0;
static uint32_t last_sample_48 = 0;

volatile HAL_StatusTypeDef g_dma_kick_49 = HAL_OK;
volatile HAL_StatusTypeDef g_dma_kick_48 = HAL_OK;
volatile uint32_t g_i2c4_error = 0;
volatile HAL_I2C_StateTypeDef g_i2c4_state = HAL_I2C_STATE_RESET;
volatile uint32_t g_memrx_cplt_hits = 0;

const uint32_t adc_buffer_read_delay_ms = 100; // Delay between ADC DMA reads


float rainbow_hue = 0.0f;
const uint32_t user_led_delay_ms = 60; // Speed for user LED cycling
const uint32_t rgb_led_delay_ms = 5;   // Speed for RGB LED rainbow effect

const uint32_t acan_msg_freq = 2; //hz

const float max_temp_c = 100.0f; // Temperature corresponding to full red
const float min_temp_c = 0.0f; // Temperature corresponding to full blue
const float open_circuit_temp_c = 200.0f; // Temperature to show for open circuit (out of range)
const uint16_t open_circuit_blink_period_ms = 500; // Blink period for open-circuit indication

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
  /* USER CODE BEGIN 2 */
  
  tc_k_init(&g_tc_k, (tc_k_cfg_t){
  .gain = 110.89f,
  .v_offset = 1.235f     /* set 0.0f if no bias */
  });

  ADC_DMA_StartAll();
  
  sk6812_init();


  TMP117_Init(&t_49, &hi2c4, 0x49);
  TMP117_Init(&t_48, &hi2c4, 0x48);

  ACAN_Init(&hfdcan3);


  // Initialize timestamps so we don't burst immediately
  last_sample_49 = HAL_GetTick();
  last_sample_48 = HAL_GetTick();

  uint32_t last_adc_poll = HAL_GetTick();

  uint32_t last_user_led_tick = HAL_GetTick();
  uint32_t last_rgb_led_tick = HAL_GetTick();
  uint32_t last_temp_OC_blink = HAL_GetTick();
  uint32_t last_acan_msg = HAL_GetTick();
  uint8_t user_led_index = 0;

  GPIO_TypeDef* ports[] = {GPIOA, GPIOB, GPIOB, GPIOB};
  uint16_t pins[] = {GPIO_PIN_10, GPIO_PIN_9, GPIO_PIN_11, GPIO_PIN_10};

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

    // User LED cycling (one at a time, smooth wave)
    if (now - last_user_led_tick >= user_led_delay_ms) {
      // Turn off previous LED
      HAL_GPIO_WritePin(ports[user_led_index], pins[user_led_index], GPIO_PIN_RESET);
      // Advance to next LED
      user_led_index = (user_led_index + 1) % 4;
      // Turn on next LED
      HAL_GPIO_WritePin(ports[user_led_index], pins[user_led_index], GPIO_PIN_SET);
      last_user_led_tick = now;

    }

    // // RGB LED rainbow effect (all LEDs, smooth cycling)
    // if (now - last_rgb_led_tick >= rgb_led_delay_ms) {
    //   for (uint16_t i = 0; i < LED_COUNT; ++i) {
    //     float hue = fmodf(rainbow_hue + (90.0f / LED_COUNT) * i, 360.0f);
    //     sk6812_set_hsi(i, hue, 1.0f, 1.0f); // full saturation, full intensity
    //   }
    //   sk6812_show();
    //   rainbow_hue += 1.0f; // speed of cycling; increase for faster effect
    //   if (rainbow_hue >= 360.0f) rainbow_hue -= 360.0f;
    //   last_rgb_led_tick = now;
    // }

    // Temprature LED representation
    float slope = (g_tmp117_c_48 - g_tmp117_c_49) / 2.0f;
    float est_temp_1 = g_tmp117_c_49 + slope * 1.0f;
    float est_temp_3 = g_tmp117_c_49 + slope * 3.0f;

    float cj_est[4] = {g_tmp117_c_48, est_temp_1, g_tmp117_c_49, est_temp_3};
    float v_tc_adc[4] = { TC1_v, TC2_v, TC3_v, TC4_v };

    tc_k_convert4(&g_tc_k, v_tc_adc, cj_est, (float*)g_tc_degC);


    //use the four temperature values to set the color of the four leds
    for (uint16_t i = 0; i < 4; ++i)
    {
      if (g_tc_degC[3-i] >= open_circuit_temp_c) // if open circuit detected
      {
        // Blink the LED between off and white
        if ((now / open_circuit_blink_period_ms) % 2 == 0) {
          sk6812_set_rgb(i, 10, 10, 10); // white
        } else {
          sk6812_set_rgb(i, 0, 0, 0); // off
        }
      }
      else {
        // Normal temperature-to-color mapping
        sk6812_set_temperature(i, g_tc_degC[3-i], min_temp_c, max_temp_c);
      }
    }
    sk6812_show();
    

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // If EXTI said something is pending, run the scheduler (starts one DMA if idle)
    // --- Kick periodic DMA reads (non-blocking) ---
    if (!t_49.busy && (now - last_sample_49) >= TMP117_SAMPLE_PERIOD_MS) {
      g_dma_kick_49 = TMP117_StartReadTemp_DMA(&t_49);
      g_i2c4_error  = hi2c4.ErrorCode;
      g_i2c4_state  = HAL_I2C_GetState(&hi2c4);
      if (g_dma_kick_49 == HAL_OK) last_sample_49 = now;
    }

    if (!t_48.busy && (now - last_sample_48) >= TMP117_SAMPLE_PERIOD_MS) {
      g_dma_kick_48 = TMP117_StartReadTemp_DMA(&t_48);
      g_i2c4_error  = hi2c4.ErrorCode;
      g_i2c4_state  = HAL_I2C_GetState(&hi2c4);
      if (g_dma_kick_48 == HAL_OK) last_sample_48 = now;
    }

    // --- Harvest results written by the DMA-complete ISR ---
    if (t_49.new_data) {
      t_49.new_data   = 0;
      g_tmp117_raw_49 = t_49.last_raw;
      g_tmp117_c_49   = t_49.last_degC;   // watch this in the debugger
    }

    if (t_48.new_data) {
      t_48.new_data   = 0;
      g_tmp117_raw_48 = t_48.last_raw;
      g_tmp117_c_48   = t_48.last_degC;   // watch this in the debugger
    }



    ///// READ ADC DMA BUFFER EVERY 10 ms /////
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

    HAL_StatusTypeDef st = ACAN_Send(&can_msg);
    uint32_t now = HAL_GetTick();
    if (st != HAL_OK) {
        
    }


    }


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
  // Hand off to the driver; it updates whichever device was active
  g_memrx_cplt_hits++;                 // prove ISR fired
  TMP117_I2C_MemRxCpltIRQ(hi2c);
}

// (Optional) If you want to recover from bus errors more aggressively later:
// void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) { /* add recovery here if needed */ }


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
