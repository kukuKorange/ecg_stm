/**
  ******************************************************************************
  * @file    transmit.c
  * @brief   数据传输模块实现
  * 
  * @details 定频传输生命体征数据:
  *          - 通过ESP8266 MQTT发送心率/血氧数据
  *          - 自动检测异常并发送报警
  ******************************************************************************
  */

#include "transmit.h"
#include "esp8266.h"
#include "max30102.h"
#include "ad8232.h"

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint16_t transmit_counter = 0;   /**< 传输计时器 (秒) */
static uint16_t alarm_counter = 0;      /**< 报警检测计时器 (秒) */

/* ECG上传相关 */
static uint16_t ecg_batch_buffer[1];    /**< ECG批次缓冲区 */
static uint32_t ecg_batch_timestamp = 0; /**< 批次时间戳 */

/*============================================================================*/
/*                              全局变量                                       */
/*============================================================================*/

volatile uint8_t transmit_flag = 0;     /**< 传输触发标志 */
volatile uint8_t alarm_check_flag = 0;  /**< 报警检测标志 */
volatile uint8_t ecg_upload_flag = 0;   /**< ECG上传触发标志（100ms一次） */

/*============================================================================*/
/*                              函数实现                                       */
/*============================================================================*/

/**
 * @brief  传输模块初始化
 */
void Transmit_Init(void)
{
    transmit_counter = 0;
    alarm_counter = 0;
    transmit_flag = 0;
    alarm_check_flag = 0;
}

/**
 * @brief  传输任务处理
 * @note   在主循环中调用，仅检查标志位后发送
 */
void Transmit_Process(void)
{
#ifdef ENABLE_MQTT_TRANSMIT
    /* 定时发送生命体征数据 */
    if (transmit_flag)
    {
        transmit_flag = 0;
        Transmit_SendVitalSign();
    }
    
    /* 报警检测（暂时关闭，减少MQTT负担）*/
    if (alarm_check_flag)
    {
        alarm_check_flag = 0;
        // Transmit_CheckAlarm();  /* 暂时关闭 */
    }
#else
    /* 传输功能已关闭，仅清除标志 */
    transmit_flag = 0;
    alarm_check_flag = 0;
#endif
}

/**
 * @brief  发送生命体征数据
 * @note   每5秒交替发送心率和血氧
 *         - 第1次调用: 发送心率
 *         - 第2次调用: 发送血氧
 *         - 循环...
 */
void Transmit_SendVitalSign(void)
{
    static uint8_t send_toggle = 0;  /* 0:心率, 1:血氧 */
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* 只有检测到手指且有有效数据时才发送 */
    if (send_toggle == 0)
    {
        /* 发送心率到 health/heartrate */
        ESP8266_SendToTopic(MQTT_TOPIC_HEARTRATE, data->heart_rate);
    }
    else
    {
        /* 发送血氧到 health/spo2 */
        ESP8266_SendToTopic(MQTT_TOPIC_SPO2, data->spo2);
    }
    
    /* 切换下次发送的内容 */
    send_toggle = !send_toggle;
}

/**
 * @brief  检查并发送报警
 */
void Transmit_CheckAlarm(void)
{
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* 未检测到手指时不报警 */
    if (!data->finger_detected)
    {
        return;
    }
    
    /* 血氧过低报警 */
    if (data->spo2 > 0 && data->spo2 < SPO2_ALARM_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_SPO2_LOW);
    }
    
    /* 心率过高报警 */
    if (data->heart_rate > HR_HIGH_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_HIGH);
    }
    
    /* 心率过低报警 */
    if (data->heart_rate > 0 && data->heart_rate < HR_LOW_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_LOW);
    }
}

/**
 * @brief  定时器回调（每秒调用一次）
 * @note   由TIM3中断调用
 */
void Transmit_TimerCallback(void)
{
    /* 传输计时 */
    transmit_counter++;
    if (transmit_counter >= TRANSMIT_INTERVAL_SEC)
    {
        transmit_counter = 0;
        transmit_flag = 1;
    }
    
    /* 报警检测计时 */
    alarm_counter++;
    if (alarm_counter >= ALARM_CHECK_INTERVAL_SEC)
    {
        alarm_counter = 0;
        alarm_check_flag = 1;
    }
}

/*============================================================================*/
/*                              ECG上传功能                                    */
/*============================================================================*/

/**
 * @brief  开始ECG上传（由按键触发）
 * @param  timestamp: 当前时间戳
 */
void Transmit_StartECGUpload(uint32_t timestamp)
{
    ecg_batch_timestamp = timestamp;
    ECG_StartUpload(timestamp);
}

/**
 * @brief  ECG上传处理（在主循环中调用）
 * @note   每100ms发送一批数据（20个采样点）
 */
void Transmit_ECGUploadProcess(void)
{
    uint16_t count;
    
    /* 检查是否有上传任务 */
    if (ECG_IsUploadComplete())
    {
        return;
    }
    
    /* 检查上传标志（100ms触发一次） */
    if (!ecg_upload_flag)
    {
        return;
    }
    ecg_upload_flag = 0;
    
    /* 获取一批数据 */
    count = ECG_GetUploadBatch(ecg_batch_buffer, 1);
    
    if (count > 0)
    {
        /* 发送到MQTT */
        ESP8266_SendECGBatch(ecg_batch_timestamp, ecg_batch_buffer, (uint8_t)count);
        
        /* 更新时间戳（每批20个点，200Hz采样 = 100ms） */
        ecg_batch_timestamp += 10;  /* 1点，每10ms发送 */
    }
}

/**
 * @brief  获取ECG上传进度
 * @retval 进度百分比 (0-100)
 */
uint8_t Transmit_GetECGProgress(void)
{
    return ECG_GetUploadProgress();
}

/**
 * @brief  检查ECG上传是否完成
 * @retval 1: 完成, 0: 进行中
 */
uint8_t Transmit_IsECGUploadComplete(void)
{
    return ECG_IsUploadComplete();
}

