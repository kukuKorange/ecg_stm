/**
  ******************************************************************************
  * @file    Timer2.c
  * @brief   定时器3驱动 - 用于ECG采样和时间测量
  * 
  * @details TIM3配置:
  *          - 时钟源: 内部时钟 72MHz
  *          - 预分频: 72 (72MHz/72 = 1MHz)
  *          - 计数周期: 10 (1MHz/10 = 100kHz中断)
  *          - 中断频率: 100000Hz (每10us一次中断)
  *          - ECG采样: 每500次中断采样一次 = 200Hz
  *          - 时间测量精度: 10us
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "timer2.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "ad8232.h"
#include "esp8266.h"
#include "key.h"
#include "max30102.h"
#include "max30102_fir.h"
#include "module/transmit/transmit.h"

/*============================ 私有变量 ============================*/

static uint32_t tim3_counter = 0;   /**< TIM3中断计数器(0-999) */

/*============================ 全局变量 ============================*/

volatile uint32_t tim3_ms_counter = 0;  /**< 10us计数器，每10us加1（用于时间测量） */

/*============================ 函数实现 ============================*/

/**
  * @brief  定时器3初始化
  * @note   配置TIM3产生1kHz中断
  */
void Timer3_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* 开启TIM3时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    
    /* 配置TIM3为内部时钟 */
    TIM_InternalClockConfig(TIM3);
    
    /* 时基单元配置 */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 10 - 1;          /* ARR = 10 */
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;     /* PSC = 7200 */
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
    
    /* 清除更新标志位（避免初始化后立即进入中断） */
    TIM_ClearFlag(TIM3, TIM_FLAG_Update);
    
    /* 使能更新中断 */
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    
    /* NVIC配置 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);
    
    /* 使能TIM3 */
    TIM_Cmd(TIM3, ENABLE);
}

/*============================ 外部变量 ============================*/

extern volatile uint8_t display_refresh_flag;  /**< 10Hz显示刷新标志 */

#ifdef ENABLE_DEBUG_PAGE
extern volatile uint8_t debug_refresh_flag;  /**< 调试页面刷新标志 */
#endif

/**
  * @brief  TIM3中断服务函数
  * @note   中断频率: 100000Hz
  *         
  *         任务分配:
  *         - 每100000次(1Hz):  更新测试计数器
  *         - 每10000次(10Hz):  调试页面刷新标志
  *         - 每500次(200Hz):   ECG采样与绘制
  *         - 每2000次(50Hz):  心率血氧数据采集
  */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET){
        tim3_counter++;
        tim3_ms_counter++;  /* 毫秒计数器（用于循环时间测量） */
        
        /* 1Hz任务: 秒计数器 + 传输模块回调 */
        if (tim3_counter >= 100000){
            test++;
            tim3_counter = 0;
            Transmit_TimerCallback();  /* 每秒调用一次传输模块 */
        }
		
		/* 50Hz任务: 心率血氧采集（非ECG页面执行） */
		if ((tim3_counter % 2000 == 0) && (current_page != PAGE_ECG)){
			max30102_process_flag = 1;
		}
        
        /* 200Hz任务: ECG采样与绘制（仅在心电图页面执行） */
        if ((tim3_counter % 500 == 0) && (current_page == PAGE_ECG)){
            ECG_SampleAndDraw();
        }
        
        /* 5Hz任务: 心率页面显示刷新 */
        if (tim3_counter % 20000 == 0){
            display_refresh_flag = 1;
        }
        
#ifdef ENABLE_DEBUG_PAGE
        /* 10Hz任务: 调试页面刷新 */
        if (tim3_counter % 10000 == 0){
            debug_refresh_flag = 1;
        }
#endif
        
        /* 清除中断标志 */
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

#endif
