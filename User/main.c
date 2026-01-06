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
#include "module/display/display.h"

/* =========================================函数声明区====================================== */

void SystemClock_Config(void);
static float Lowpass(float X_last, float X_new, float K);

/* =========================================变量定义区====================================== */
/* 注: HR_CACHE_NUMS 和 PPG_DATA_THRESHOLD 已在 kconfig.h 中定义为 HR_CACHE_NUMS 和 PPG_DATA_THRESHOLD */
uint8_t max30102_int_flag = 0;      /* 中断标志 */

float ppg_data_cache_RED[HR_CACHE_NUMS] = {0};  /* 缓存区 */
float ppg_data_cache_IR[HR_CACHE_NUMS] = {0};   /* 缓存区 */

uint16_t Chart[250];
uint16_t Chart_1[120];
uint16_t HR_new = 0;
uint16_t HR_last = 0;
uint16_t SpO2_value = 0;  /* 血氧值 */

static uint8_t last_page = 0xFF;  /* 上一次的页面，用于检测页面切换 */

#ifdef ENABLE_DEBUG_PAGE
/* 调试页面刷新控制 */
volatile uint8_t debug_refresh_flag = 0;  /* 调试页面刷新标志（由定时器置位） */

/* 循环时间测量（使用TIM3的100kHz计数器，单位：10us） */
static uint32_t loop_start_ms = 0;              /* 循环开始时的计数值 */
uint32_t display_loop_time_ms = 0;              /* 一次循环时间（10us）- 供显示模块使用 */
uint32_t display_loop_time_max_ms = 0;          /* 最大循环时间（10us）- 供显示模块使用 */
#endif

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
    
    /* 按键初始化 */
    Key_Init();
    
    while(1)
    {
#ifdef ENABLE_DEBUG_PAGE
        /* ==================== 记录循环开始时间（使用TIM3的ms计数器） ==================== */
        loop_start_ms = tim3_ms_counter;
#endif
        
        /* ==================== 按键处理 ==================== */
        Key_Process();
        
        /* ==================== 页面切换检测 ==================== */
        if (current_page != last_page)
        {
            OLED_Clear();
            last_page = current_page;
#ifdef ENABLE_DEBUG_PAGE
            display_loop_time_max_ms = 0;  /* 切换页面时重置最大时间 */
#endif
        }
        
        /* ==================== 根据当前页面显示内容 ==================== */
        switch (current_page)
        {
            case PAGE_HEARTRATE:
                Display_Page0_HeartRate();
                break;
                
            case PAGE_ECG:
                Display_Page1_ECG();
                break;
                
#ifdef ENABLE_DEBUG_PAGE
            case PAGE_DEBUG:
                /* 调试页面：10Hz刷新 */
                if (debug_refresh_flag)
                {
                    debug_refresh_flag = 0;
                    Display_Page2_Debug();
                }
                break;
#endif
                
            default:
                current_page = PAGE_HEARTRATE;
                break;
        }
        
        /* ==================== 心率血氧数据采集（后台运行）==================== */
        if(1)  /* 中断信号产生 */
        { 
            max30102_int_flag = 0;
            max30102_fifo_read(max30102_data);      /* 读取数据 */
        
            ir_max30102_fir(&max30102_data[0], &fir_output[0]);
            red_max30102_fir(&max30102_data[1], &fir_output[1]);
        
            if((max30102_data[0] > PPG_DATA_THRESHOLD) && (max30102_data[1] > PPG_DATA_THRESHOLD))
            {       
                ppg_data_cache_IR[cache_counter] = fir_output[0];
                ppg_data_cache_RED[cache_counter] = fir_output[1];
                cache_counter++;
                LED1_ON
            }
            else
            {
                cache_counter = 0;
                LED1_OFF
                HR_new = 0;
            }

            if(cache_counter >= HR_CACHE_NUMS)
            {
                cache_counter = 0;
                HR_new = Lowpass(HR_last, max30102_getHeartRate(ppg_data_cache_IR, HR_CACHE_NUMS), 0.6);
                SpO2_value = max30102_getSpO2(ppg_data_cache_IR, ppg_data_cache_RED, HR_CACHE_NUMS);
                HR_last = max30102_getHeartRate(ppg_data_cache_IR, HR_CACHE_NUMS);
                // ESP8266_Send("HeartRate", (int)HR_new);
            }
        }
        
        /* ==================== LED报警 ==================== */
#ifdef ENABLE_LED_INDICATOR
        if(HR_new >= HR_ALARM_THRESHOLD)
        {
            LED2_ON
        }
        else
        {
            LED2_OFF
        }
#endif
        
#ifdef ENABLE_DEBUG_PAGE
        /* ==================== 计算循环时间（使用TIM3的100kHz计数器） ==================== */
        display_loop_time_ms = tim3_ms_counter - loop_start_ms;
        
        /* 更新最大循环时间 */
        if (display_loop_time_ms > display_loop_time_max_ms)
        {
            display_loop_time_max_ms = display_loop_time_ms;
        }
#endif
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
 * @brief  系统时钟配置 (72MHz)
 */
void SystemClock_Config(void)
{
    ErrorStatus HSEStartUpStatus;
    
    RCC_DeInit();
    RCC_HSEConfig(RCC_HSE_ON);
    HSEStartUpStatus = RCC_WaitForHSEStartUp();
    
    if(HSEStartUpStatus == SUCCESS)
    {
        FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
        FLASH_SetLatency(FLASH_Latency_2);
        RCC_HCLKConfig(RCC_SYSCLK_Div1);
        RCC_PCLK2Config(RCC_HCLK_Div1);
        RCC_PCLK1Config(RCC_HCLK_Div2);
        RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
        RCC_PLLCmd(ENABLE);
        while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
        RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
        while(RCC_GetSYSCLKSource() != 0x08);
    }
    else
    {
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
