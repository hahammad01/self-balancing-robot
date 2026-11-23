/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motors.h"
#include <stdio.h>
#include <string.h>
#include "mpu6500.h"
#include "tilt.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TICK_HZ            200U                  /* TIM6 update rate              */
#define TICK_DT_S          (1.0f / TICK_HZ)      /* nominal tick period, 5 ms     */
#define TELEMETRY_DIV      20U                   /* angle line every 20 ticks     */
#define STATS_DIV          200U                  /* stats line every 200 ticks    */
#define IMU_FAIL_LIMIT     5U                    /* consecutive failed reads      */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Incremented by the TIM6 interrupt, consumed by the main loop. A counter rather
   than a flag, so ticks that arrive while the loop is busy are counted, not lost. */
volatile uint32_t g_ticks_pending = 0;

Tilt     g_tilt;
char     g_msg[112];

/* Loop statistics, reported once per second on a line starting with '#'. */
uint32_t g_ticks_total   = 0;   /* ticks consumed since start                    */
uint32_t g_ticks_missed  = 0;   /* ticks that arrived before the previous one was processed */
uint32_t g_work_max_cyc  = 0;   /* longest IMU read + filter, CPU cycles         */
uint32_t g_loop_max_cyc  = 0;   /* longest pass including UART output, CPU cycles */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void uart_print(const char *s);
static void halt_with_blink(const char *reason);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        g_ticks_pending++;   /* the work itself runs in main() */
    }
}

static void uart_print(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), 20);
}

/* Motors off, report the reason, blink the red LED forever. */
static void halt_with_blink(const char *reason)
{
    Motors_Stop();
    Motors_Enable(0);
    HAL_GPIO_WritePin(LED_CAL_GPIO_Port, LED_CAL_Pin, GPIO_PIN_RESET);
    uart_print(reason);
    while (1) {
        HAL_GPIO_TogglePin(LED_FALL_GPIO_Port, LED_FALL_Pin);
        HAL_Delay(100);
    }
}

/* DWT cycle counter, used to time the loop body (168 cycles = 1 us at 168 MHz). */
static void cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
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
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  Motors_Init();
  Motors_Stop();
  cycle_counter_init();

  /* IMU bring-up: orange LED on until calibration is done. */
  HAL_GPIO_WritePin(LED_CAL_GPIO_Port, LED_CAL_Pin, GPIO_PIN_SET);
  uint8_t who = 0;
  if (MPU_Init(&hi2c1, &who) != HAL_OK) {
      sprintf(g_msg, "# IMU init failed (WHO_AM_I=0x%02X)\r\n", who);
      halt_with_blink(g_msg);
  }
  if (Tilt_Calibrate(&g_tilt, &hi2c1) != HAL_OK) {    /* robot must be still */
      sprintf(g_msg, "# gyro calibration failed: %lu of %lu reads valid\r\n",
              (unsigned long)g_tilt.cal_valid, (unsigned long)TILT_CAL_SAMPLES);
      halt_with_blink(g_msg);
  }
  HAL_GPIO_WritePin(LED_CAL_GPIO_Port, LED_CAL_Pin, GPIO_PIN_RESET);

  sprintf(g_msg, "# WHO_AM_I=0x%02X gyro_bias=%.1f LSB (%lu valid) tau=%.2f s\r\n",
          who, g_tilt.gyro_bias, (unsigned long)g_tilt.cal_valid, TILT_TAU_S);
  uart_print(g_msg);
  uart_print("# time_ms,angle_deg,rate_dps\r\n");
  uart_print("# stats: #S,ticks_total,ticks_missed,work_max_us,loop_max_us,imu_read_errors\r\n");

  /* Start the 200 Hz tick last, once everything else is ready. */
  HAL_TIM_Base_Start_IT(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      /* Take all pending ticks atomically. */
      __disable_irq();
      uint32_t n = g_ticks_pending;
      g_ticks_pending = 0;
      __enable_irq();
      if (n == 0)
          continue;

      uint32_t t_start = DWT->CYCCNT;
      g_ticks_missed += n - 1;          /* n > 1 means the loop fell behind */
      g_ticks_total  += n;

      /* dt follows the ticks actually elapsed, so a late pass does not under-integrate. */
      static uint32_t imu_fail_run = 0;
      if (Tilt_Update(&g_tilt, &hi2c1, (float)n * TICK_DT_S) == HAL_OK) {
          imu_fail_run = 0;
      } else if (++imu_fail_run >= IMU_FAIL_LIMIT) {
          halt_with_blink("# IMU: 5 consecutive failed reads\r\n");
      }
      uint32_t work = DWT->CYCCNT - t_start;
      if (work > g_work_max_cyc) g_work_max_cyc = work;

      /* Angle telemetry, about 10 Hz. */
      /* stats_count starts half a telemetry period in, so the two lines are never sent
         in the same pass: at 115200 baud the two lines together take about 4.5 ms of a 5 ms tick. */
      static uint32_t tel_count = 0, stats_count = TELEMETRY_DIV / 2U;
      tel_count += n;
      if (tel_count >= TELEMETRY_DIV) {
          tel_count = 0;
          sprintf(g_msg, "%lu,%.2f,%.2f\r\n",
                  (unsigned long)HAL_GetTick(), g_tilt.angle, g_tilt.rate);
          uart_print(g_msg);
      }

      /* Loop statistics, once per second. ticks_total is compared with the PC clock by
         control/log_serial.py; this is the independent check of the 200 Hz rate. */
      stats_count += n;
      if (stats_count >= STATS_DIV) {
          stats_count = 0;
          uint32_t cyc_per_us = SystemCoreClock / 1000000U;
          sprintf(g_msg, "#S,%lu,%lu,%lu,%lu,%lu\r\n",
                  (unsigned long)g_ticks_total, (unsigned long)g_ticks_missed,
                  (unsigned long)(g_work_max_cyc / cyc_per_us),
                  (unsigned long)(g_loop_max_cyc / cyc_per_us),
                  (unsigned long)g_tilt.read_errors);
          uart_print(g_msg);
          g_work_max_cyc = 0;
          g_loop_max_cyc = 0;
      }

      uint32_t loop = DWT->CYCCNT - t_start;
      if (loop > g_loop_max_cyc) g_loop_max_cyc = loop;
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* Driver to standby first, so the motors cannot keep their last command. */
  STBY_GPIO_Port->BSRR = (uint32_t)STBY_Pin << 16U;
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
