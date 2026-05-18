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

/* 外设驱动 */
#include "led/bsp_led.h" 
#include "oled.h"
#include "Timer2.h"
#include "usart2.h"
#include "esp8266.h"
#include "AD.h"
#include "ad8232.h"
#include "key.h"

/* 功能模块 */
#ifdef USE_MAX30102
#include "max30102.h"
#include "max30102_fir.h"
#endif
#ifdef USE_ECG_SIM
#include "ecg_sim/ecg_sim.h"
#endif
#ifdef USE_DS18B20
#include "ds18b20/ds18b20.h"
#endif
#include "module/display/display.h"
#include "module/transmit/transmit.h"

/* =========================================函数声明区====================================== */

void SystemClock_Config(void);

/* =========================================变量定义区====================================== */

/* 10Hz显示刷新标志（由定时器置位） */
volatile uint8_t display_refresh_flag = 0;

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
int main(void){
    /* 配置NVIC优先级分组 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    /* 初始化系统时钟为72MHz */
    SystemClock_Config();
    
    
    /* 初始化LED */
    LED_GPIO_Config();
    
    /* 初始化MAX30102心率血氧模块（可通过kconfig.h中USE_MAX30102关闭） */
#ifdef USE_MAX30102
    max30102_init();
    max30102_fir_init();
#endif
    
    /* 初始化DS18B20温度计模块（可通过kconfig.h中USE_DS18B20关闭） */
#ifdef USE_DS18B20
    DS18B20_Init();
#endif
    
    OLED_Init();
    
    usart2_init(115200);     /* 串口2初始化(PA2/PA3)为115200 esp-01s通信 */
    ESP8266_Init();
    Transmit_Init();         /* 传输模块初始化 */
    
    /* 心电图外设配置 */
    AD_Init();
    AD8232Init();
    Timer3_Init();

    /* ECG信号模拟器初始化（USE_ECG_SIM启用时替代硬件ADC采样） */
#ifdef USE_ECG_SIM
    ECG_Sim_Init(ECG_SIM_BPM);
#endif
    
    /* 按键初始化 */
    Key_Init();

    
    while(1){
#ifdef ENABLE_DEBUG_PAGE
        /* ==================== 记录循环开始时间（使用TIM3的ms计数器） ==================== */
        loop_start_ms = tim3_ms_counter;
#endif
        
        /* ==================== 按键处理 ==================== */
        Key_Process();
        
        /* ==================== 心率血氧数据采集（50Hz，由定时器触发） ==================== */
#ifdef USE_MAX30102
        if (max30102_process_flag)
        {
            max30102_process_flag = 0;
            MAX30102_Process();
        }
#endif
        
        /* ==================== 温度数据采集（10Hz，由定时器触发） ==================== */
#ifdef USE_DS18B20
        if (display_refresh_flag)
        {
            DS18B20_Process();  /* 周期性处理DS18B20，管理非阻塞式温度转换 */
        }
#endif
        
        /* ==================== 页面显示更新 ==================== */
        Display_Update();
        
        /* ==================== LED状态指示 ==================== */
#ifdef ENABLE_LED_INDICATOR
        LED_StatusUpdate();
#endif
        
        /* ==================== 数据传输处理（含ECG上传） ==================== */
        Transmit_Process();
        
#ifdef ENABLE_DEBUG_PAGE
        /* ==================== 计算循环时间（使用TIM3的100kHz计数器） ==================== */
        display_loop_time_ms = tim3_ms_counter - loop_start_ms;
        
        /* 更新最大循环时间 */
        if (display_loop_time_ms > display_loop_time_max_ms){
            display_loop_time_max_ms = display_loop_time_ms;
        }
#endif
    }
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
