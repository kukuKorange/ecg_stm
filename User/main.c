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

/* =========================================函数声明区====================================== */

void SystemClock_Config(void);
static float Lowpass(float X_last, float X_new, float K);
static void Display_Page0_HeartRate(void);
static void Display_Page1_ECG(void);

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
uint16_t SpO2_value = 0;  /* 血氧值 */

static uint8_t last_page = 0xFF;  /* 上一次的页面，用于检测页面切换 */

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
        /* ==================== 按键处理 ==================== */
        Key_Process();
        
        /* ==================== 页面切换检测 ==================== */
        if (current_page != last_page)
        {
            OLED_Clear();
            last_page = current_page;
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

            if(cache_counter >= CACHE_NUMS)
            {
                cache_counter = 0;
                HR_new = Lowpass(HR_last, max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS), 0.6);
                SpO2_value = max30102_getSpO2(ppg_data_cache_IR, ppg_data_cache_RED, CACHE_NUMS);
                HR_last = max30102_getHeartRate(ppg_data_cache_IR, CACHE_NUMS);
                ESP8266_Send("HeartRate", (int)HR_new);
            }
        }
        
        /* ==================== LED报警 ==================== */
        if(HR_new >= 70)
        {
            LED2_ON
        }
        else
        {
            LED2_OFF
        }
    }
}

/**
 * @brief  页面0: 心率血氧显示
 * 
 * @details 显示内容:
 *          ┌────────────────────────┐
 *          │ ♥ Heart Rate & SpO2    │
 *          │                        │
 *          │   HR: 75 bpm           │
 *          │   SpO2: 98 %           │
 *          │                        │
 *          │ [<] Page 1/2     [>]   │
 *          └────────────────────────┘
 */
static void Display_Page0_HeartRate(void)
{
    /* 标题 */
    OLED_ShowString(0, 0, "Heart Rate & SpO2", OLED_6X8);
    
    /* 分隔线 */
    OLED_DrawLine(0, 10, 127, 10);
    
    /* 心率显示 */
    OLED_ShowString(10, 16, "HR:", OLED_8X16);
    OLED_ShowNum(50, 16, HR_new, 3, OLED_8X16);
    OLED_ShowString(80, 16, "bpm", OLED_8X16);
    
    /* 血氧显示 */
    OLED_ShowString(10, 36, "SpO2:", OLED_8X16);
    OLED_ShowNum(60, 36, SpO2_value, 3, OLED_8X16);
    OLED_ShowString(100, 36, "%", OLED_8X16);
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "1/2", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    OLED_Update();
}

/**
 * @brief  页面1: 心电图显示
 * 
 * @details 显示内容:
 *          ┌────────────────────────┐
 *          │ ECG Monitor            │
 *          │ ┌──────────────────┐   │
 *          │ │    /\    /\      │   │
 *          │ │___/  \__/  \___  │   │
 *          │ └──────────────────┘   │
 *          │ [<] Page 2/2     [>]   │
 *          └────────────────────────┘
 */
static void Display_Page1_ECG(void)
{
    /* 标题 */
    OLED_ShowString(0, 0, "ECG Monitor", OLED_6X8);
    
    /* 坐标系绘制 */
    OLED_DrawLine(1, 54, 120, 54);     /* X轴 */
    OLED_DrawLine(1, 10, 1, 54);       /* Y轴 */
    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);    /* Y轴箭头 */
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);  /* X轴箭头 */
    
    /* 显示心电数据（由Timer3中断更新） */
    OLED_ShowNum(100, 0, test, 3, OLED_6X8);
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "2/2", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    OLED_Update();
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
