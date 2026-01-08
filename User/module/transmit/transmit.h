/**
  ******************************************************************************
  * @file    transmit.h
  * @brief   数据传输模块头文件
  * 
  * @details 定频传输生命体征数据到服务器:
  *          - 心率 (bpm)
  *          - 血氧 (%)
  *          - 报警信息
  ******************************************************************************
  */

#ifndef __TRANSMIT_H
#define __TRANSMIT_H

#include <stdint.h>
#include "kconfig.h"

/*============================================================================*/
/*                              传输配置                                       */
/*============================================================================*/

/**
 * @brief  启用MQTT数据传输
 * @note   注释此行可完全关闭传输功能
 */
#define ENABLE_MQTT_TRANSMIT

/**
 * @brief  数据传输间隔 (秒)
 * @note   每隔此时间发送一次生命体征数据
 */
#define TRANSMIT_INTERVAL_SEC       5   /* 每5秒发送一次，心率和血氧交替 */

/**
 * @brief  报警检测间隔 (秒)
 */
#define ALARM_CHECK_INTERVAL_SEC    10

/*============================================================================*/
/*                              外部变量                                       */
/*============================================================================*/

/**
 * @brief  传输触发标志（由定时器置位）
 */
extern volatile uint8_t transmit_flag;

/**
 * @brief  报警检测标志（由定时器置位）
 */
extern volatile uint8_t alarm_check_flag;

/*============================================================================*/
/*                              函数声明                                       */
/*============================================================================*/

/**
 * @brief  传输模块初始化
 */
void Transmit_Init(void);

/**
 * @brief  传输任务处理
 * @note   在主循环中调用，检查标志并发送数据
 */
void Transmit_Process(void);

/**
 * @brief  发送生命体征数据
 * @note   发送心率和血氧到 MQTT 服务器
 */
void Transmit_SendVitalSign(void);

/**
 * @brief  检查并发送报警
 * @note   检测异常情况并发送报警信息
 */
void Transmit_CheckAlarm(void);

/**
 * @brief  定时器回调（由TIM3中断调用）
 * @note   每秒调用一次，用于计时
 */
void Transmit_TimerCallback(void);

/*============================ ECG上传接口 ============================*/

/* ECG上传标志（由定时器设置，100ms一次） */
extern volatile uint8_t ecg_upload_flag;

/**
 * @brief  开始ECG上传（由按键触发）
 * @param  timestamp: 当前时间戳
 */
void Transmit_StartECGUpload(uint32_t timestamp);

/**
 * @brief  ECG上传处理（在主循环中调用）
 * @note   每100ms发送一批数据（20个采样点）
 */
void Transmit_ECGUploadProcess(void);

/**
 * @brief  获取ECG上传进度
 * @retval 进度百分比 (0-100)
 */
uint8_t Transmit_GetECGProgress(void);

/**
 * @brief  检查ECG上传是否完成
 * @retval 1: 完成, 0: 进行中
 */
uint8_t Transmit_IsECGUploadComplete(void);

#endif /* __TRANSMIT_H */

