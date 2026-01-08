/**
  ******************************************************************************
  * @file    esp8266.h
  * @brief   ESP8266 WiFi模块驱动头文件
  ******************************************************************************
  */

#ifndef __ESP8266_H
#define __ESP8266_H

#ifdef USE_STDPERIPH_DRIVER

#include "stdint.h"
#include "string.h"

#undef USE_HAL_DRIVER  /* 禁用HAL库 */

/*============================ 服务器模式选择 ============================*/

/**
  * @brief  MQTT服务器模式
  *         0 - 自建MQTT服务器 (Mosquitto/EMQX等)
  *         1 - 阿里云IoT平台
  */
#define MQTT_USE_ALIYUN     0

/*============================ WiFi配置 ============================*/

#define WIFI_NAME           "5132"
#define WIFI_PASSWORD       "ttst4qrj"

/*============================ MQTT通用配置 ============================*/

/* MQTT主题 */
#define MQTT_TOPIC_HEARTRATE    "health/heartrate"    /**< 心率数据主题 */
#define MQTT_TOPIC_SPO2         "health/spo2"         /**< 血氧数据主题 */
#define MQTT_TOPIC_TEMPERATURE  "health/temperature"  /**< 体温数据主题 */
#define MQTT_TOPIC_ECG          "health/ecg"          /**< 心电数据主题 */
#define MQTT_TOPIC_ALARM        "health/alarm"        /**< 报警信息主题 */

/* 兼容旧代码 */
#define MQTT_TOPIC_VITAL    MQTT_TOPIC_HEARTRATE
#define MQTT_TOPIC_POST     MQTT_TOPIC_HEARTRATE
#define POST                MQTT_TOPIC_POST
#define MQTT_PROPERTY_NAME  "heartRate"
#define POST_NAME           MQTT_PROPERTY_NAME

/*============================ MQTT服务器配置 ============================*/

#if (MQTT_USE_ALIYUN == 1)
/*--------------------- 阿里云IoT配置 ---------------------*/

/* 产品信息 */
#define ALIYUN_PRODUCT_KEY      "k16e93sVlLN"
#define ALIYUN_DEVICE_NAME      "esp01s"
#define ALIYUN_DEVICE_SECRET    "4ac4f12ef4b322f3212b281e7addd35477f3a2ebf27fccf80dac6678201d4b2c"
#define ALIYUN_REGION           "cn-shanghai"

/* MQTT AT指令 */
#define MQTT_USERCFG    "AT+MQTTUSERCFG=0,1,\"NULL\",\"esp01s&k16e93sVlLN\",\"4ac4f12ef4b322f3212b281e7addd35477f3a2ebf27fccf80dac6678201d4b2c\",0,0,\"\""
#define MQTT_CLIENTID   "AT+MQTTCLIENTID=0,\"k16e93sVlLN.esp01s|securemode=2\\,signmethod=hmacsha256\\,timestamp=1744624949805|\""
#define MQTT_CONN       "AT+MQTTCONN=0,\"iot-06z00dazkl34gjg.mqtt.iothub.aliyuncs.com\",1883,1"

#else
/*--------------------- 自建服务器配置 ---------------------*/

/* 服务器地址和端口 */
#define MQTT_BROKER_HOST    "47.115.148.200"     /* 修改为你的服务器IP */
#define MQTT_BROKER_PORT    "1883"

/* 客户端配置 */
#define MQTT_CLIENT_ID      "ecg_stm32_c8t6"     /* 客户端ID（必须唯一）*/

/* MQTT AT指令 (无认证模式，用户名密码为空) */
#define MQTT_USERCFG    "AT+MQTTUSERCFG=0,1,\"" MQTT_CLIENT_ID "\",\"\",\"\",0,0,\"\""
#define MQTT_CLIENTID   ""  /* 自建服务器不需要单独设置ClientID */
#define MQTT_CONN       "AT+MQTTCONN=0,\"" MQTT_BROKER_HOST "\"," MQTT_BROKER_PORT ",1"

#endif /* MQTT_USE_ALIYUN */

/* 兼容旧代码的宏名 */
#define MQTT_set    MQTT_USERCFG
#define MQTT_Client MQTT_CLIENTID
#define MQTT_Pass   MQTT_CONN

/*============================ 外部变量 ============================*/

extern unsigned char Property_Data[];  /**< 云端属性数据缓冲区 */

/*============================ 函数声明 ============================*/

/**
  * @brief  ESP8266模块初始化
  */
void ESP8266_Init(void);

/**
  * @brief  向ESP8266发送AT指令
  * @param  cmd: AT指令字符串
  * @param  ack: 期望应答字符串
  * @param  waittime: 超时时间(单位:10ms)
  * @retval 0:成功 1:失败
  */
uint8_t esp8266_send_cmd(char *cmd, char *ack, uint16_t waittime);

/**
  * @brief  检查ESP8266应答内容
  * @param  str: 期望字符串
  * @retval 0:未找到 1:找到
  */
uint8_t esp8266_check_cmd(char *str);

/**
  * @brief  向指定主题发送数据
  * @param  topic: MQTT主题 (如 MQTT_TOPIC_HEARTRATE)
  * @param  Data: 数据值
  */
void ESP8266_SendToTopic(const char *topic, int Data);

/**
  * @brief  向服务器发送数据（通用，兼容旧代码）
  * @param  property: 属性名称
  * @param  Data: 属性值
  */
void ESP8266_Send(char *property, int Data);

/**
  * @brief  发送生命体征数据
  * @param  heart_rate: 心率 (bpm)
  * @param  spo2: 血氧饱和度 (%)
  * @note   发送到 ecg/vitalsign 主题
  *         JSON格式: {"heartRate":xx,"oxygenSaturation":xx}
  */
void ESP8266_SendVitalSign(uint16_t heart_rate, uint16_t spo2);

/**
  * @brief  发送报警信息
  * @param  alarm_type: 报警类型
  *         - 0: 血氧过低
  *         - 1: 心率过高
  *         - 2: 心率过低
  *         - 3: 体温异常
  *         - 4: 心电异常
  * @param  severity: 严重程度 (1-5)
  * @note   发送到 ecg/alarm 主题
  */
void ESP8266_SendAlarm(uint8_t alarm_type, uint8_t severity);

/**
  * @brief  接收服务器下发数据
  * @param  PRO: 要查找的属性名称
  */
void ESP8266_Received(char *PRO);

#endif /* USE_STDPERIPH_DRIVER */

#endif /* __ESP8266_H */
