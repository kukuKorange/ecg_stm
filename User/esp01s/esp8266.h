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
#define WIFI_PASSWORD       "53288837"

/*============================ MQTT通用配置 ============================*/

/* MQTT主题 */
#define MQTT_TOPIC_POST     "/ecg/data"
#define MQTT_TOPIC_SUB      "/ecg/cmd"
#define MQTT_PROPERTY_NAME  "heartrate"

/* 兼容旧代码 */
#define POST        MQTT_TOPIC_POST
#define POST_NAME   MQTT_PROPERTY_NAME

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
#define MQTT_BROKER_HOST    "192.168.1.100"     /* 修改为你的服务器IP */
#define MQTT_BROKER_PORT    "1883"

/* 认证信息 (无认证可留空) */
#define MQTT_USERNAME       ""                   /* 用户名，无认证留空 */
#define MQTT_PASSWORD       ""                   /* 密码，无认证留空 */
#define MQTT_CLIENT_ID      "ecg_stm32"          /* 客户端ID */

/* MQTT AT指令 */
#define MQTT_USERCFG    "AT+MQTTUSERCFG=0,1,\"" MQTT_CLIENT_ID "\",\"" MQTT_USERNAME "\",\"" MQTT_PASSWORD "\",0,0,\"\""
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
  * @brief  向服务器发送数据
  * @param  property: 属性名称
  * @param  Data: 属性值
  */
void ESP8266_Send(char *property, int Data);

/**
  * @brief  接收服务器下发数据
  * @param  PRO: 要查找的属性名称
  */
void ESP8266_Received(char *PRO);

#endif /* USE_STDPERIPH_DRIVER */

#endif /* __ESP8266_H */
