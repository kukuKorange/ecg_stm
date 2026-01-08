/**
  ******************************************************************************
  * @file    display.h
  * @brief   OLED显示模块头文件
  * 
  * @details 封装各页面的显示功能:
  *          - 页面0: 心率血氧显示
  *          - 页面1: 心电图显示
  *          - 页面2: 调试页面（可选）
  ******************************************************************************
  */

#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <stdint.h>
#include "kconfig.h"
#include "max30102.h"

/*============================================================================*/
/*                              外部变量声明                                   */
/*============================================================================*/

/* 页面控制（来自Key.c） */
extern uint8_t current_page;

/* 10Hz显示刷新标志（来自main.c，由Timer3置位） */
extern volatile uint8_t display_refresh_flag;

#ifdef ENABLE_DEBUG_PAGE
/* 调试数据（来自main.c） */
extern uint32_t display_loop_time_ms;      /**< 循环时间 (10us) */
extern uint32_t display_loop_time_max_ms;  /**< 最大循环时间 (10us) */

/* 调试页面刷新标志（来自main.c） */
extern volatile uint8_t debug_refresh_flag;
#endif

/*============================================================================*/
/*                              函数声明                                       */
/*============================================================================*/

/**
 * @brief  显示更新（主入口函数）
 * @note   处理页面切换检测并调用对应页面显示函数
 */
void Display_Update(void);

/**
 * @brief  页面0: 心率血氧显示
 * @note   显示内容:
 *         - 标题: "Heart Rate & SpO2"
 *         - 心率值 (bpm)
 *         - 血氧值 (%)
 *         - 页码指示
 */
void Display_Page0_HeartRate(void);

/**
 * @brief  页面1: 心电图显示
 * @note   显示内容:
 *         - 标题: "ECG Monitor"
 *         - XY坐标系
 *         - 心电波形（由Timer3中断绘制）
 *         - 运行时间
 *         - 页码指示
 */
void Display_Page1_ECG(void);

#ifdef ENABLE_DEBUG_PAGE
/**
 * @brief  页面2: 调试页面
 * @note   显示内容（10Hz刷新）:
 *         - 循环时间（当前/最大）
 *         - ADC原始值
 *         - 心率/血氧值
 *         - 运行时间
 */
void Display_Page2_Debug(void);
#endif

#endif /* __DISPLAY_H */

