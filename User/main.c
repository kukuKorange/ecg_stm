/**
 ******************************************************************************
 * @file    main.c
 * @author  fire
 * @version V1.0
 * @date    2024-xx-xx
 * @brief   检测心率与血氧
 ******************************************************************************
 ******************************************************************************
 */

#include "main.h"
#include "stm32f1xx.h"
#include <stdlib.h>
#include "usart/bsp_debug_usart.h"
#include "led/bsp_led.h" 
#include "i2c/bsp_i2c.h"
#include "max30102.h"
#include "max30102_fir.h"
#include "oled.h"
#include "Timer1.h"
#include "Timer2.h"
#include "esp8266.h"
#include "usart2.h"
#include "AD.h"
#include "ad8232.h"
#include "key.h"

/* =========================================函数申明区====================================== */

void SystemClock_Config(void);
float Lowpass(float X_last, float X_new, float K);

/* =========================================变量定义区====================================== */
#define CACHE_NUMS 150//缓存数
#define PPG_DATA_THRESHOLD 100000 	//检测阈值
uint8_t max30102_int_flag=0;  		//中断标志

float ppg_data_cache_RED[CACHE_NUMS]={0};  //缓存区
float ppg_data_cache_IR[CACHE_NUMS]={0};  //缓存区
I2C_HandleTypeDef I2C_Handle;


uint16_t Chart[250];
uint16_t Chart_1[120];
uint16_t HR_new = 0;
uint16_t HR_last = 0;

uint8_t KeyNum;

/**
 * @brief  主函数
 * @param  无
 * @retval 无
 */
int main(void)
{
   uint16_t cache_counter=0;  //缓存计数器
	float max30102_data[2],fir_output[2];
    
    HAL_Init();
	/* 初始化系统时钟为72MHz */
    SystemClock_Config();
    /* 初始化USART 配置模式为 115200 8-N-1 */
//    DEBUG_USART_Config();
//	/* 初始化LED */
	LED_GPIO_Config();
    
  /* 初始化心率血氧模块 */
	max30102_init();
  /* FIR滤波计算初始化 */
	max30102_fir_init();
	OLED_Init();
	
	usart2_init(115200);	 //串口2初始化(PA2/PA3)为115200 esp-01s通信
	ESP8266_Init();

	/* 心电图外设配置 */
	AD_Init();
	AD8232Init();
	Timer3_Init();
	/* 心电图外设配置 */
	
	Key_Init();
	
	
	/* 坐标系绘制 */
	OLED_DrawLine(1,62,120,62);
	OLED_DrawLine(1,8,1,62);
	OLED_DrawTriangle(1,6,0,8,2,8,OLED_UNFILLED);
	OLED_DrawTriangle(120,63,120,61,123,62,OLED_UNFILLED);
	OLED_Update();
	/* 坐标系绘制 */
	
	while(1)
	{
//		KeyNum = Key_GetNum();

		
		OLED_ShowNum(1,20,test,2,OLED_6X8);
		OLED_Update();

//		if(GetConnect())
//		{
//			OLED_ShowString(8,1,"Connected",OLED_6X8);
//		}
//		else
//		{
//			OLED_ShowString(8,1,"Disconnect",OLED_6X8);
//		}
		
		if(1)			//中断信号产生
		{ 
				max30102_int_flag = 0;
				max30102_fifo_read(max30102_data);		//读取数据
		
				ir_max30102_fir(&max30102_data[0],&fir_output[0]);//实测ir数据采集在前面，red数据在后面
				red_max30102_fir(&max30102_data[1],&fir_output[1]);  //滤波
		
				if((max30102_data[0]>PPG_DATA_THRESHOLD)&&(max30102_data[1]>PPG_DATA_THRESHOLD))  //大于阈值，说明传感器有接触
				{		
						ppg_data_cache_IR[cache_counter]=fir_output[0];
						ppg_data_cache_RED[cache_counter]=fir_output[1];
						cache_counter++;
						LED1_ON
				}
				else				//小于阈值
				{
						cache_counter=0;
						LED1_OFF
						HR_new = 0;
				}


				if(cache_counter>=CACHE_NUMS)  //收集满了数据
				{
//                printf("心率：%d  次/min   ",max30102_getHeartRate(ppg_data_cache_IR,CACHE_NUMS));
//                printf("血氧：%.2f  %%\n",max30102_getSpO2(ppg_data_cache_IR,ppg_data_cache_RED,CACHE_NUMS));
						cache_counter=0;
						HR_new = Lowpass(HR_last,max30102_getHeartRate(ppg_data_cache_IR,CACHE_NUMS),0.6);
						OLED_ShowNum(5,1,max30102_getHeartRate(ppg_data_cache_IR,CACHE_NUMS),2,OLED_6X8);
						OLED_ShowNum(20,1,max30102_getSpO2(ppg_data_cache_IR,ppg_data_cache_RED,CACHE_NUMS),2,OLED_6X8);
						OLED_Update();
						HR_last = max30102_getHeartRate(ppg_data_cache_IR,CACHE_NUMS);
						ESP8266_Send("HeartRate",(int)HR_new);
				}
				
				
		}
		
		if(HR_new >= 70) //检测心率值是否超出80
		{
			LED2_ON //蜂鸣器开
		}
		else
		{
			LED2_OFF
		}
				
				
	}
}




void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin==MAX30102_INT_Pin)
    {
        max30102_int_flag=1;
    }
}
/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (HSE)
 *            SYSCLK(Hz)                     = 72000000
 *            HCLK(Hz)                       = 72000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 2
 *            APB2 Prescaler                 = 1
 *            HSE Frequency(Hz)              = 8000000
 *            HSE PREDIV1                    = 1
 *            PLLMUL                         = 9
 *            Flash Latency(WS)              = 2
 * @param  None
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef clkinitstruct = {0};
    RCC_OscInitTypeDef oscinitstruct = {0};

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscinitstruct.HSEState = RCC_HSE_ON;
    oscinitstruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscinitstruct.PLL.PLLState = RCC_PLL_ON;
    oscinitstruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscinitstruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oscinitstruct) != HAL_OK)
    {
        /* Initialization Error */
        while (1)
            ;
    }

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
       clocks dividers */
    clkinitstruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    clkinitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkinitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clkinitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
    clkinitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clkinitstruct, FLASH_LATENCY_2) != HAL_OK)
    {
        /* Initialization Error */
        while (1)
            ;
    }
}



void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
