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
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "usart.h"
#include "screen_app.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DAC_SPS 6000000
#define FFT_SIZE 4096
#define PF1_TO_PF12_MASK  0x7FF8
/* TIM1 读时刻相对 TIM2(ADC 时钟)上升沿的滞后：取 TIM1 ARR/2=21，
   使 DMA 读落在 ADC 时钟周期中点(~250ns)，避开 ADC 输出翻转窗 */
#define TIM1_READ_PHASE_OFFSET 21
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
//flag
volatile uint8_t buma=0;
volatile uint8_t ad9238_ok_flag=0;
volatile uint8_t data_1_flag=1;
volatile uint8_t rebegin_enable_flag=0;
//

/* H7 通信相关标志（当前已注释通信，保留声明以免后续恢复） */
//volatile uint8_t tx_enable_flag=0;
//volatile uint8_t tx_cplt_flag=0;
//volatile uint8_t rx_enable_flag=0;
//volatile uint8_t rx_cplt_flag=0;

extern DMA_HandleTypeDef hdma_tim1_up; 
__attribute__((aligned(16))) uint16_t adc_data_3[FFT_SIZE];
//__attribute__((aligned(32))) uint32_t rxBuffer[6]={0};

volatile uint8_t a=0;
/* TIM1 读相位偏移（等效采样时随 ARR 更新） */
volatile uint16_t tim1_read_phase_offset = TIM1_READ_PHASE_OFFSET;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
  ad9238_ok_flag=1;
}

/* H7 通信回调（当前已注释通信，保留以备恢复） */
//void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
//{
//  if(GPIO_Pin==GPIO_PIN_15){
//    tx_enable_flag=1;
//  }else if(GPIO_Pin==GPIO_PIN_0){
//    rx_enable_flag=1;
//  }
//}
//
//void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
//{
//  if(hspi->Instance==SPI1){
//    tx_cplt_flag=1;
//  }
//}
//
//void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
//{
//  if(hspi->Instance==SPI1){
//    rx_cplt_flag=1;
//  }
//}

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
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_RESET);        //始终开启片选

  /* ---- AD9238 采集时序配置（保留） ---- */
	TIM2->PSC =2-1;
	TIM2->ARR = (42000000U + DAC_SPS / 2U) / DAC_SPS - 1U;
  TIM2->EGR |= TIM_EGR_UG;
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (TIM2->ARR+1)*2/3); 
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  TIM1->PSC =2-1;
	TIM1->ARR = (84000000U + DAC_SPS / 2U) / DAC_SPS - 1U;
  TIM1->EGR |= TIM_EGR_UG;

	hdma_tim1_up.XferCpltCallback = HAL_DMA_XferCpltCallback;
	HAL_DMA_Start_IT(&hdma_tim1_up, (uint32_t)&GPIOF->IDR ,(uint32_t)adc_data_3,  sizeof(adc_data_3)/sizeof(uint16_t));
__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);

	HAL_Delay(1);
	__HAL_TIM_ENABLE(&htim2);
	__NOP();
	__NOP();__NOP();__NOP();__NOP();

	__HAL_TIM_SET_COUNTER(&htim1, tim1_read_phase_offset);
	__HAL_TIM_ENABLE(&htim1);

  /* ---- 屏幕显示初始化（屏工程逻辑，内部含 500ms 屏启动等待）---- */
  ScreenApp_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 屏幕显示任务：当前数据源为 wave_table 测试数据（SCREEN_TEST_ENABLE=1） */
    ScreenApp_Task();

    /* ---- AD9238 采集处理（保留） ---- */
    if(ad9238_ok_flag==1){
      ad9238_ok_flag=0;
      __HAL_TIM_DISABLE(&htim1);
      __HAL_TIM_DISABLE(&htim2);
      __HAL_TIM_SET_COUNTER(&htim2, 0);__HAL_TIM_SET_COUNTER(&htim1, tim1_read_phase_offset);

      /* 12 位数据转换（buma=1 符号扩展，buma=0 直接右移） */
      if(buma==1){
        for (uint16_t i = 0; i < FFT_SIZE; i++)
        {
          uint16_t raw_data = ((adc_data_3[i] & 0x7FF8) >> 3);
          if (raw_data & (0x0800)) {
            adc_data_3[i] = (int16_t)(raw_data | 0xF000); 
          } else {
            adc_data_3[i] = (int16_t)raw_data;
          }
        }
      }else if(buma==0){
        for (uint16_t i = 0; i < FFT_SIZE; i++){
          uint16_t raw_data = ((adc_data_3[i] & 0x7FF8) >> 3);
          adc_data_3[i] = raw_data;
        }
      }

      /* 转换完成，自动重新开始采集 */
      rebegin_enable_flag=1;
    }

    /* ---- H7 通信（已注释，屏显示仅用 wave_table 测试数据）---- */
    /*
    if(data_1_flag==1 && tx_enable_flag==1){
      data_1_flag=0;
      tx_enable_flag=0;
      HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)adc_data_3, sizeof(adc_data_3)/sizeof(uint16_t));
    }
    if(tx_cplt_flag==1 && rx_enable_flag==1){
      tx_cplt_flag=0;
      rx_enable_flag=0;
      HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)rxBuffer, sizeof(rxBuffer)/sizeof(uint16_t));
    }
    if(rx_cplt_flag==1){
      rx_cplt_flag=0;
      rebegin_enable_flag=1;
    }
    */

    if(rebegin_enable_flag==1){
      //重新开始采集
      rebegin_enable_flag=0;
      __HAL_TIM_ENABLE(&htim2);
      __NOP();
      __NOP();__NOP();__NOP();__NOP();
      __HAL_TIM_SET_COUNTER(&htim1, tim1_read_phase_offset);
      __HAL_TIM_ENABLE(&htim1);

      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
      HAL_Delay(2);
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;
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
