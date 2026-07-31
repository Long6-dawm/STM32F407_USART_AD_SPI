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
#include "hmi_tjc.h"
#include "wave_table.h"

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

/* 等效采样：1 个等效周期采样点数 */
#define EQ_POINTS_PER_PERIOD 256u
/* 显示用点数 = 3 个等效周期 */
#define EQ_DISPLAY_POINTS (3u * EQ_POINTS_PER_PERIOD)
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
volatile uint8_t tx_enable_flag=0;
volatile uint8_t tx_cplt_flag=0;
volatile uint8_t rx_enable_flag=0;
volatile uint8_t rx_cplt_flag=0;
volatile uint8_t rebegin_enable_flag=0;
//

extern DMA_HandleTypeDef hdma_tim1_up; 
__attribute__((aligned(16))) uint16_t adc_data_3[FFT_SIZE];
__attribute__((aligned(32))) uint32_t rxBuffer[6]={0};

volatile uint8_t a=0;
/* TIM1 读相位偏移（等效采样时随 ARR 更新） */
volatile uint16_t tim1_read_phase_offset = TIM1_READ_PHASE_OFFSET;

/* TJC 显示状态 */
static uint8_t s_wave_periods = 3u;      /* 显示 1 周期 / 3 周期 */
static uint8_t s_key_stable_level = 1u;  /* 按键稳定电平（上拉=1 未按下） */
static uint8_t s_key_last_raw = 1u;
static uint32_t s_key_last_change_ms = 0u;
static uint8_t s_display_ready = 0u;     /* 一帧数据已就绪待刷屏 */
static uint32_t s_last_refresh_ms = 0u;  /* TJC 刷屏节流 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
  ad9238_ok_flag=1;
	
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
 HAL_Delay(5);          //消抖
  if(GPIO_Pin==GPIO_PIN_15){
    tx_enable_flag=1;
  }else if(GPIO_Pin==GPIO_PIN_0){
    rx_enable_flag=1;
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if(hspi->Instance==SPI1){
    tx_cplt_flag=1;
  }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if(hspi->Instance==SPI1){
    rx_cplt_flag=1;
    //接收完成
  }
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  从 H7 返回的 rxBuffer 中解析基频值（单位 Hz）。
  *         当前 H7 只发占位数据，此函数为占位实现，待与 H7 协议对齐后修改。
  *         占位约定：rxBuffer[0] 为 uint32，直接表示基频(Hz)。
  * @retval 基频(Hz)，非法时返回 0
  */
static uint32_t GetBaseFrequencyHz(void)
{
  return (uint32_t)rxBuffer[0];
}

/**
  * @brief  根据基频 f0 配置等效采样率 fs = f0*256/255。
  *         每个采样点相位前进 1/256 周期 => 1 个等效周期 256 点。
  *         TIM2 = AD9238 采样时钟，TIM1 = 同频率触发 DMA 读 GPIOF。
  * @param  f0_hz 基频（Hz）
  */
static void ConfigEquivalentSampling(uint32_t f0_hz)
{
  uint32_t fs;
  uint32_t arr2;
  uint32_t arr1;

  if (f0_hz == 0u)
  {
    return;
  }

  fs = (uint32_t)(((uint64_t)f0_hz * EQ_POINTS_PER_PERIOD) /
                  (EQ_POINTS_PER_PERIOD - 1u));
  if (fs == 0u)
  {
    return;
  }

  /* TIM2：APB1 定时器钟 84MHz，div=2（与原 42MHz 计数钟一致），32 位 ARR */
  uint32_t div2 = 2u;
  arr2 = (84000000U / div2 / fs) - 1u;
  TIM2->PSC = div2 - 1u;
  TIM2->ARR = arr2;
  TIM2->EGR |= TIM_EGR_UG;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (TIM2->ARR + 1u) * 2u / 3u);

  /* TIM1：APB2 定时器钟 168MHz，16 位 ARR（<=65535）。
     div 为分频系数（PSC 寄存器值 = div-1），计数钟 = 168MHz/div。
     初始 div=2（与原 84MHz 计数钟一致），低频时增大 div 保证 ARR 落在 16 位范围。 */
  uint32_t div = 2u;
  arr1 = (168000000U / div / fs) - 1u;
  while ((arr1 > 65535u) && (div < 65536u))
  {
    div++;
    arr1 = (168000000U / div / fs) - 1u;
  }
  if (arr1 > 65535u)
  {
    arr1 = 65535u;
  }
  TIM1->PSC = div - 1u;
  TIM1->ARR = arr1;
  TIM1->EGR |= TIM_EGR_UG;

  /* 保持 DMA 读落在 ADC 时钟周期中点 */
  tim1_read_phase_offset = (TIM1->ARR + 1u) / 2u;
}

/* TJC 屏曲线高度（与屏工程 SCREEN_CURVE_HEIGHT 一致） */
#define SCREEN_CURVE_HEIGHT 210u
#define SCREEN_WAVE_CTRL "s_wave"
#define SCREEN_KEY_DEBOUNCE_MS 30u

/**
  * @brief  将采集数据归一化并发送到 TJC 屏（addt 协议）。
  *         按当前 s_wave_periods 显示 768 点(3 周期) 或取中间 256 点(1 周期)。
  */
static void SendDisplayWave(void)
{
  uint16_t pixel[EQ_DISPLAY_POINTS];
  uint8_t  px[EQ_DISPLAY_POINTS];
  uint16_t count = EQ_DISPLAY_POINTS;
  uint16_t start = 0u;
  uint16_t i;
  int32_t  vmax = -32767;
  int32_t  vmin = 32767;
  uint32_t span;

  if (s_wave_periods == 1u)
  {
    /* 1 周期：取 3 周期数据中间 1/3 */
    count = EQ_POINTS_PER_PERIOD;
    start = EQ_DISPLAY_POINTS / 3u;
  }

  for (i = 0u; i < count; i++)
  {
    int32_t v = (int16_t)adc_data_3[start + i];
    if (v > vmax) { vmax = v; }
    if (v < vmin) { vmin = v; }
    pixel[i] = (uint16_t)v;
  }

  span = (uint32_t)(vmax - vmin);
  if (span == 0u) { span = 1u; }

  for (i = 0u; i < count; i++)
  {
    int32_t y = (int32_t)(((uint32_t)(pixel[i] - (uint16_t)vmin) * SCREEN_CURVE_HEIGHT) / span);
    if (y < 0) { y = 0; }
    if (y > (int32_t)SCREEN_CURVE_HEIGHT) { y = SCREEN_CURVE_HEIGHT; }
    px[i] = (uint8_t)y;
  }

  HMI_ClearWave(SCREEN_WAVE_CTRL, 255u);
  HMI_Addt_Send(SCREEN_WAVE_CTRL, 0u, px, count);
}

/**
  * @brief  PE0 按键轮询消抖（低电平按下），切换 1 周期 / 3 周期显示。
  * @param  now_ms 当前毫秒时间戳
  */
static void ScreenKey_Scan(uint32_t now_ms)
{
  uint8_t raw = (HAL_GPIO_ReadPin(Single_MCU_GPIO_Port, Single_MCU_Pin) == GPIO_PIN_RESET) ? 0u : 1u;

  if (raw != s_key_last_raw)
  {
    s_key_last_raw = raw;
    s_key_last_change_ms = now_ms;
    return;
  }

  if ((raw != s_key_stable_level) && ((now_ms - s_key_last_change_ms) >= SCREEN_KEY_DEBOUNCE_MS))
  {
    s_key_stable_level = raw;
    if (s_key_stable_level == 0u)
    {
      /* 按下：切换显示周期 */
      s_wave_periods = (s_wave_periods == 1u) ? 3u : 1u;
      HMI_SetText("T_num", (s_wave_periods == 1u) ? "1" : "3");
    }
  }
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
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_15, GPIO_PIN_RESET);        //始终开启片选

	TIM2->PSC =2-1;
 
	TIM2->ARR = (42000000U + DAC_SPS / 2U) / DAC_SPS - 1U;
  TIM2->EGR |= TIM_EGR_UG;
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (TIM2->ARR+1)*2/3); 
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);



TIM1->PSC =2-1;
  //TIM1->PSC =84-1;
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

  /* TJC 屏初始化 */
  HMI_GotoPage("page0");
  HMI_ClearWave("s_wave", 255u);
  HMI_SetText("T_num", "3");

  /* 测试模式：先显示预置 wave_table 数据（768 点 / 3 周期） */
  {
    uint8_t tpx[WAVE_TABLE_LENGTH];
    uint16_t i;
    uint16_t tmax = 1u;
    uint32_t tspan;
    uint16_t tcount = WAVE_TABLE_LENGTH;
    uint16_t tstart = 0u;

    for (i = 0u; i < WAVE_TABLE_LENGTH; i++)
    {
      if (g_wave_table[i] > tmax) { tmax = g_wave_table[i]; }
    }
    tspan = (uint32_t)tmax;

    if (s_wave_periods == 1u)
    {
      tcount = WAVE_TABLE_PERIOD_POINTS;
      tstart = WAVE_TABLE_LENGTH / 3u;
    }
    for (i = 0u; i < tcount; i++)
    {
      uint32_t y = ((uint32_t)g_wave_table[tstart + i] * SCREEN_CURVE_HEIGHT) / tspan;
      if (y > SCREEN_CURVE_HEIGHT) { y = SCREEN_CURVE_HEIGHT; }
      tpx[i] = (uint8_t)y;
    }
    HMI_ClearWave(SCREEN_WAVE_CTRL, 255u);
    HMI_Addt_Send(SCREEN_WAVE_CTRL, 0u, tpx, tcount);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    ScreenKey_Scan(HAL_GetTick());

    /* 数据就绪：转换完成后节流刷屏（TJC addt 发送耗时约 70ms，避免每帧阻塞） */
    if (s_display_ready == 1u)
    {
      s_display_ready = 0u;
      if ((HAL_GetTick() - s_last_refresh_ms) >= 300u)
      {
        s_last_refresh_ms = HAL_GetTick();
        SendDisplayWave();
      }
    }
		
    if(ad9238_ok_flag==1){
      ad9238_ok_flag=0;
 __HAL_TIM_DISABLE(&htim1);
	__HAL_TIM_DISABLE(&htim2);
	__HAL_TIM_SET_COUNTER(&htim2, 0);__HAL_TIM_SET_COUNTER(&htim1, tim1_read_phase_offset);

  if(buma==1){
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
      // 1. 提取 12 位数据并右移对齐
        uint16_t raw_data = ((adc_data_3[i] & 0x7FF8) >> 3);
        
			
        // 2. 12位补码符号扩展到16位
        // 如果 Bit 11 是 1（负数），把高 4 位全补 1；如果是 0（正数），高 4 位补 0
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
	//完成转化
	//准备发送
  data_1_flag=1;
  s_display_ready=1;
    }

    //开始发射
    if(data_1_flag==1 && tx_enable_flag==1){
      data_1_flag=0;
      tx_enable_flag=0;
      HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)adc_data_3, sizeof(adc_data_3)/sizeof(uint16_t));
      //发送数据1
    }

//开始接收
    if(tx_cplt_flag==1 && rx_enable_flag==1){
     tx_cplt_flag=0;
      rx_enable_flag=0;
     HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)rxBuffer, sizeof(rxBuffer)/sizeof(uint16_t));
    }

if(rx_cplt_flag==1){
      rx_cplt_flag=0;
      //接收完成
	//处理接收值：根据 H7 返回的基频重新配置等效采样率
	{
	  uint32_t f0 = GetBaseFrequencyHz();
	  if (f0 > 0u)
	  {
	    ConfigEquivalentSampling(f0);
	  }
	}
	rebegin_enable_flag=1;
    }

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

      //通知从机重新开始了

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
