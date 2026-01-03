/**
 ******************************************************************************
 * @file    main.c
 * @author  fire
 * @version V1.0
 * @date    2024-xx-xx
 * @brief   检测心率与血氧 - 使用STM32标准库
 ******************************************************************************
 */

#include "main.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_flash.h"
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
static float Lowpass(float X_last, float X_new, float K);

/* =========================================变量定义区====================================== */
#define CACHE_NUMS 150  /* 缓存数 */
#define PPG_DATA_THRESHOLD 100000   /* 检测阈值 */
uint8_t max30102_int_flag = 0;      /* 中断标志 */

float ppg_data_cache_RED[CACHE_NUMS] = {0};  /* 缓存区 */
float ppg_data_cache_IR[CACHE_NUMS] = {0};   /* 缓存区 */

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
    uint16_t cache_counter = 0;  /* 缓存计数器 */
    float max30102_data[2], fir_output[2];
    
    /* 配置NVIC优先级分组 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    /* 初始化系统时钟为72MHz */
    SystemClock_Config();
    
    /* 初始化LED */
    LED_GPIO_Config();
    
    /* 初始化心率血氧模块 */
    max30102_init();
    
    /* FIR滤波计算初始化 */
    max30102_fir_init();
    
    OLED_Init();
    
    usart2_init(115200);     /* 串口2初始化(PA2/PA3)为115200 esp-01s通信 */
    ESP8266_Init();

    /* 心电图外设配置 */
    AD_Init();
    AD8232Init();
    Timer3_Init();
    /* 心电图外设配置 */
    
    Key_Init();
    
    /* 坐标系绘制 */
    OLED_DrawLine(1, 62, 120, 62);
    OLED_DrawLine(1, 8, 1, 62);
    OLED_DrawTriangle(1, 6, 0, 8, 2, 8, OLED_UNFILLED);
    OLED_DrawTriangle(120, 63, 120, 61, 123, 62, OLED_UNFILLED);
    OLED_Update();
    /* 坐标系绘制 */
    
    while(1)
    {
        OLED_ShowNum(1, 20, test, 2, OLED_6X8);
        OLED_Update();
        
        if(1)  /* 中断信号产生 */
        { 
            max30102_int_flag = 0;
            max30102_fifo_read(max30102_data);      /* 读取数据 */
        
            ir_max30102_fir(&max30102_data[0], &fir_output[0]);  /* 实测ir数据采集在前面，red数据在后面 */
            red_max30102_fir(&max30102_data[1], &fir_output[1]); /* 滤波 */
        
            if((max30102_data[0] > PPG_DATA_THRESHOLD) && (max30102_data[1] > PPG_DATA_THRESHOLD))  /* 大于阈值，说明传感器有接触 */
            {       
                ppg_data_cache_IR[cache_counter] = fir_output[0];
                ppg_data_cache_RED[cache_counter] = fir_output[1];
                cache_counter++;
                LED1_ON
            }
            else  /* 小于阈值 */
            {
                cache_counter = 0;
                LED1_OFF
                HR_new = 0;
            }

            if(cache_counter >= CACHE_NUMS)  /* 收集满了数据 */
            {
                cache_counter = 0;
                HR_new = Lowpass(HR_last, max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS), 0.6);
                OLED_ShowNum(5, 1, max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS), 2, OLED_6X8);
                OLED_ShowNum(20, 1, max30102_getSpO2(ppg_data_cache_IR, ppg_data_cache_RED, CACHE_NUMS), 2, OLED_6X8);
                OLED_Update();
                HR_last = max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS);
                ESP8266_Send("HeartRate", (int)HR_new);
            }
        }
        
        if(HR_new >= 70)  /* 检测心率值是否超出80 */
        {
            LED2_ON  /* 蜂鸣器开 */
        }
        else
        {
            LED2_OFF
        }
    }
}

/**
 * @brief  低通滤波函数
 */
static float Lowpass(float X_last, float X_new, float K)
{
    return X_last + K * (X_new - X_last);
}

/**
 * @brief  系统时钟配置
 *         系统时钟配置如下:
 *            系统时钟源               = PLL (HSE)
 *            SYSCLK(Hz)              = 72000000
 *            HCLK(Hz)                = 72000000
 *            AHB Prescaler           = 1
 *            APB1 Prescaler          = 2
 *            APB2 Prescaler          = 1
 *            HSE Frequency(Hz)       = 8000000
 *            PLL MUL                 = 9
 *            Flash Latency(WS)       = 2
 * @param  None
 * @retval None
 */
void SystemClock_Config(void)
{
    ErrorStatus HSEStartUpStatus;
    
    /* 复位RCC时钟配置为默认状态 */
    RCC_DeInit();
    
    /* 使能外部高速晶振HSE */
    RCC_HSEConfig(RCC_HSE_ON);
    
    /* 等待HSE启动稳定 */
    HSEStartUpStatus = RCC_WaitForHSEStartUp();
    
    if(HSEStartUpStatus == SUCCESS)
    {
        /* 使能Flash预取缓冲区 */
        FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
        
        /* 设置Flash等待周期，SYSCLK > 48MHz需要2个等待周期 */
        FLASH_SetLatency(FLASH_Latency_2);
        
        /* 设置AHB时钟(HCLK) = SYSCLK */
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        
        /* 设置APB2时钟(PCLK2) = HCLK */
        RCC_PCLK2Config(RCC_HCLK_Div1);
        
        /* 设置APB1时钟(PCLK1) = HCLK/2 */
        RCC_PCLK1Config(RCC_HCLK_Div2);
        
        /* 配置PLL: PLLCLK = HSE * 9 = 72MHz */
        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        
        /* 使能PLL */
        RCC_PLLCmd(ENABLE);
        
        /* 等待PLL稳定 */
        while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
        
        /* 选择PLL作为系统时钟源 */
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        
        /* 等待PLL被选为系统时钟源 */
        while(RCC_GetSYSCLKSource() != 0x08);
    }
    else
    {
        /* HSE启动失败，用户可在此添加错误处理 */
        while(1);
    }
}

/**
 * @brief  错误处理函数
 */
void Error_Handler(void)
{
    while(1);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
