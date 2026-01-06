/**
  ******************************************************************************
  * @file    AD8232.h
  * @brief   AD8232心电模块驱动头文件
  ******************************************************************************
  */

#ifndef __AD8232_H
#define __AD8232_H

#include "stdint.h"

/*============================ 外部变量 ============================*/
extern uint16_t ecg_data[500];      /**< ECG数据缓冲区 */
extern uint16_t map_upload[130];    /**< 上传数据缓冲区 */
extern uint16_t ecg_index;          /**< ECG数据索引 */
extern uint16_t test;               /**< 测试计数器 */

/*============================ 函数声明 ============================*/

/**
 * @brief  AD8232初始化
 */
void AD8232Init(void);

/**
 * @brief  获取电极连接状态
 * @retval 1: 已连接, 0: 未连接
 */
uint8_t GetConnect(void);

/**
 * @brief  获取心率值
 * @param  array: 数据数组
 * @param  length: 数组长度
 * @retval 心率值
 */
uint8_t GetHeartRate(uint16_t *array, uint16_t length);

/**
 * @brief  ECG数据采集与绘制（在定时器中断中调用）
 * @note   采样率200Hz，每5ms调用一次
 */
void ECG_SampleAndDraw(void);

/**
 * @brief  ECG显示区域清除并重绘坐标轴
 */
void ECG_ClearAndRedraw(void);

/**
 * @brief  绘制ECG图表
 * @param  Chart: 数据数组
 * @param  Width: 线宽
 */
void DrawChart(uint16_t Chart[], uint8_t Width);

/**
 * @brief  图表数据优化（平均滤波）
 * @param  R: 原始数据
 * @param  Chart: 输出数据
 */
void ChartOptimize(uint16_t *R, uint16_t *Chart);

#endif
