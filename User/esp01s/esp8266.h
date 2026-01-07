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

/*============================ WiFi配置 ============================*/

#define WIFI_NAME       "5132"
#define WIFI_PASSWORD   "53288837"

/*============================ MQTT配置 ============================*/

/* MQTT主题 */
#define MQTT_TOPIC_POST     "/k16e93sVlLN/esp01s/user/esp01s"
#define MQTT_PROPERTY_NAME  "LightSwitch"

/* 兼容旧代码 */
#define post        MQTT_TOPIC_POST
#define post_name   MQTT_PROPERTY_NAME

/* MQTT用户配置 (阿里云IoT) */
#define MQTT_set    "AT+MQTTUSERCFG=0,1,\"NULL\",\"esp01s&k16e93sVlLN\",\"4ac4f12ef4b322f3212b281e7addd35477f3a2ebf27fccf80dac6678201d4b2c\",0,0,\"\""

/* MQTT客户端ID */
#define MQTT_Client "AT+MQTTCLIENTID=0,\"k16e93sVlLN.esp01s|securemode=2\\,signmethod=hmacsha256\\,timestamp=1744624949805|\""

/* MQTT服务器连接 */
#define MQTT_Pass   "AT+MQTTCONN=0,\"iot-06z00dazkl34gjg.mqtt.iothub.aliyuncs.com\",1883,1"

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
  * @brief  向云端发送数据
  * @param  property: 属性名称
  * @param  Data: 属性值
  */
void ESP8266_Send(char *property, int Data);

/**
  * @brief  接收云端下发数据
  * @param  PRO: 要查找的属性名称
  */
void ESP8266_Received(char *PRO);

#endif /* USE_STDPERIPH_DRIVER */

#endif /* __ESP8266_H */
