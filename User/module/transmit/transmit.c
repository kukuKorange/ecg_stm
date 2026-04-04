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
#ifdef USE_MAX30102
#include "max30102.h"
#endif
#include "ad8232.h"

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint16_t transmit_counter = 0;   /**< 传输计时器 (秒) */
static uint16_t alarm_counter = 0;      /**< 报警检测计时器 (秒) */

/* ECG上传相关 */
static uint16_t ecg_upload_count = 0;      /**< 上传计数器 (0-599) */
static uint32_t ecg_batch_timestamp = 0;   /**< 批次时间戳 */

/*============================================================================*/
/*                              全局变量                                       */
/*============================================================================*/

volatile uint8_t transmit_flag = 0;     /**< 传输触发标志 */
volatile uint8_t alarm_check_flag = 0;  /**< 报警检测标志 */
volatile uint8_t ecg_upload_flag = 0;   /**< ECG上传触发标志（100ms一次） */

/*============================================================================*/
/*                              私有函数声明                                    */
/*============================================================================*/

static void Transmit_ECGUploadProcess(void);

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
 * @note   在主循环中调用，处理所有传输任务
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
    
    /* ECG上传处理（按键触发后分批上传） */
    Transmit_ECGUploadProcess();
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
#ifdef USE_MAX30102
    static uint8_t send_toggle = 0;  /* 0:心率, 1:血氧 */
    MAX30102_Data_t *data = MAX30102_GetData();
    
    if (send_toggle == 0)
    {
        ESP8266_SendToTopic(MQTT_TOPIC_HEARTRATE, data->heart_rate);
    }
    else
    {
        ESP8266_SendToTopic(MQTT_TOPIC_SPO2, data->spo2);
    }
    send_toggle = !send_toggle;
#else
    /* 无MAX30102，仅发送ECG计算的心率 */
    ESP8266_SendToTopic(MQTT_TOPIC_HEARTRATE, (uint16_t)ECG_GetHeartRate());
#endif
}

/**
 * @brief  检查并发送报警
 */
void Transmit_CheckAlarm(void)
{
#ifdef USE_MAX30102
    MAX30102_Data_t *data = MAX30102_GetData();
    
    if (!data->finger_detected)
    {
        return;
    }
    
    if (data->spo2 > 0 && data->spo2 < SPO2_ALARM_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_SPO2_LOW);
    }
    
    if (data->heart_rate > HR_HIGH_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_HIGH);
    }
    
    if (data->heart_rate > 0 && data->heart_rate < HR_LOW_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_LOW);
    }
#else
    /* 无MAX30102，仅基于ECG心率报警（无血氧报警） */
    uint8_t ecg_hr = ECG_GetHeartRate();
    
    if (ecg_hr > HR_HIGH_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_HIGH);
    }
    
    if (ecg_hr > 0 && ecg_hr < HR_LOW_THRESHOLD)
    {
        ESP8266_Send("alarm", ALARM_TYPE_HR_LOW);
    }
#endif
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
 * @retval 1: 开始上传, 0: 无完整数据可上传
 */
uint8_t Transmit_StartECGUpload(uint32_t timestamp)
{
    if (ECG_StartUpload(timestamp))
    {
        ecg_batch_timestamp = timestamp;
        ecg_upload_count = 0;  /* 从第0个点开始 */
        return 1;
    }
    return 0;  /* 还没有完整的600点数据 */
}

/**
 * @brief  ECG上传处理（内部函数）
 * @note   每次上传1个数据点，由定时器标志触发
 */
static void Transmit_ECGUploadProcess(void)
{
    uint16_t *buffer;
    
    /* 检查是否有上传任务 */
    if (ECG_IsUploadComplete())
    {
        return;
    }
    
    /* 检查上传标志 */
    if (!ecg_upload_flag)
    {
        return;
    }
    ecg_upload_flag = 0;
    
    /* 检查是否上传完成 */
    if (ecg_upload_count >= 600)
    {
        ECG_StopUpload();  /* 标记上传完成 */
        return;
    }
    
    /* 获取缓冲区指针，发送当前点 */
    buffer = ECG_GetUploadBuffer();
    ESP8266_SendECGBatch(ecg_batch_timestamp, &buffer[ecg_upload_count], 1);
    
    /* 计数器+1，时间戳+5ms（200Hz采样） */
    ecg_upload_count++;
    ecg_batch_timestamp += 5;
}

/**
 * @brief  获取ECG上传进度
 * @retval 进度百分比 (0-100)
 */
uint8_t Transmit_GetECGProgress(void)
{
    if (ECG_IsUploadComplete())
    {
        return 100;
    }
    return (uint8_t)((ecg_upload_count * 100) / 600);
}

/**
 * @brief  检查ECG上传是否完成
 * @retval 1: 完成, 0: 进行中
 */
uint8_t Transmit_IsECGUploadComplete(void)
{
    return ECG_IsUploadComplete();
}

