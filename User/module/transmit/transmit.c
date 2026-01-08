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

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint16_t transmit_counter = 0;   /**< 传输计时器 (秒) */
static uint16_t alarm_counter = 0;      /**< 报警检测计时器 (秒) */

/*============================================================================*/
/*                              全局变量                                       */
/*============================================================================*/

volatile uint8_t transmit_flag = 0;     /**< 传输触发标志 */
volatile uint8_t alarm_check_flag = 0;  /**< 报警检测标志 */

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

