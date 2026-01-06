/**
  ******************************************************************************
  * @file    Timer2.c
  * @brief   定时器3驱动 - 用于ECG采样
  * 
  * @details TIM3配置:
  *          - 时钟源: 内部时钟 72MHz
  *          - 预分频: 7200 (72MHz/7200 = 10kHz)
  *          - 计数周期: 10 (10kHz/10 = 1kHz中断)
  *          - 中断频率: 1000Hz
  *          - ECG采样: 每5次中断采样一次 = 200Hz
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "timer2.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "ad8232.h"
#include "key.h"

/*============================ 私有变量 ============================*/

static uint16_t tim3_counter = 0;   /**< TIM3中断计数器 */

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
    TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;     /* PSC = 7200 */
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

/**
  * @brief  TIM3中断服务函数
  * @note   中断频率: 1000Hz
  *         
  *         任务分配:
  *         - 每1000次(1秒): 更新测试计数器
  *         - 每5次(200Hz): ECG采样与绘制
  */
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET)
    {
        tim3_counter++;
        
        /* 1Hz任务: 秒计数器 */
        if (tim3_counter >= 1000)
        {
            test++;
            tim3_counter = 0;
        }
        
        /* 200Hz任务: ECG采样与绘制（仅在心电图页面执行） */
        if ((tim3_counter % 5 == 0) && (current_page == PAGE_ECG))
        {
            ECG_SampleAndDraw();
        }
        
        /* 清除中断标志 */
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

#endif
