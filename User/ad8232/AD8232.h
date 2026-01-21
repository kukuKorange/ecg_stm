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

/* ECG上传相关 */
extern uint8_t  ecg_upload_active;       /**< 上传进行中标志 */

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
 * @brief  获取ECG计算的心率
 * @retval 心率值 (bpm)，0表示尚未检测到有效心率
 */
uint8_t ECG_GetHeartRate(void);

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

/*============================ ECG上传接口 ============================*/

/**
 * @brief  开始ECG数据上传
 * @param  timestamp: 数据起始时间戳
 * @retval 1: 开始上传, 0: 无完整数据可上传
 */
uint8_t ECG_StartUpload(uint32_t timestamp);

/**
 * @brief  检查是否有完整数据可上传
 * @retval 1: 有完整600点数据, 0: 无
 */
uint8_t ECG_IsDataReady(void);

/**
 * @brief  读取上传缓冲区指定索引的数据
 * @param  index: 数据索引 (0-599)
 * @retval 该索引处的ECG值
 */
uint16_t ECG_GetUploadData(uint16_t index);

/**
 * @brief  获取上传缓冲区指针
 * @retval 上传缓冲区起始地址
 */
uint16_t* ECG_GetUploadBuffer(void);

/**
 * @brief  停止ECG数据上传
 */
void ECG_StopUpload(void);

/**
 * @brief  获取待上传的数据量
 * @retval 缓存中的数据点数
 */
uint16_t ECG_GetUploadDataCount(void);

/**
 * @brief  获取一批ECG数据用于上传
 * @param  batch_data: 输出缓冲区
 * @param  batch_size: 请求的批次大小
 * @retval 实际获取的数据点数
 */
uint16_t ECG_GetUploadBatch(uint16_t *batch_data, uint16_t batch_size);

/**
 * @brief  获取上传进度
 * @retval 进度百分比 (0-100)
 */
uint8_t ECG_GetUploadProgress(void);

/**
 * @brief  检查上传是否完成
 * @retval 1: 完成, 0: 进行中
 */
uint8_t ECG_IsUploadComplete(void);

#endif
