/**
  ******************************************************************************
  * @file    esp8266.c
  * @brief   ESP8266 WiFi模块驱动 (ESP-01S)
  * 
  * @details 通过AT指令控制ESP8266模块，实现：
  *          - WiFi Station模式连接路由器
  *          - MQTT协议连接服务器（支持阿里云IoT/自建服务器）
  *          - 传感器数据上传与服务器指令接收
  * 
  * @note    在esp8266.h中设置 MQTT_USE_ALIYUN 选择服务器模式：
  *          - 0: 自建MQTT服务器 (Mosquitto/EMQX等)
  *          - 1: 阿里云IoT平台
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "esp8266.h"
#include "usart2.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

/*============================ 宏定义 ============================*/

#undef USE_HAL_DRIVER  /* 禁用HAL库 */

/*============================ 全局变量 ============================*/

unsigned char Property_Data[5];  /**< 云端属性数据缓冲区 */

/*============================ 私有函数 ============================*/

/**
  * @brief  毫秒级延时函数
  * @param  ms: 延时毫秒数
  * @note   使用SysTick定时器实现精确延时
  */
static void delay_ms(uint16_t ms)
{
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;  /* 动态计算重载值 */
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    
    while (ms--)
    {
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
        SysTick->CTRL &= ~SysTick_CTRL_COUNTFLAG_Msk;  /* 清除标志 */
    }
    SysTick->CTRL = 0;  /* 关闭定时器 */
}

/*============================ 公共函数 ============================*/

/**
  * @brief  ESP8266模块初始化
  * @note   初始化流程：
  *         1. 测试AT通信
  *         2. 设置Station模式
  *         3. 连接WiFi路由器
  *         4. 配置MQTT并连接服务器
  */
void ESP8266_Init(void)
{
    uint8_t retry;
    
    /* 等待模块启动 */
    delay_ms(2000);
    
    /* 测试AT通信 */
    for (retry = 0; retry < 3; retry++)
    {
        if (esp8266_send_cmd("AT", "OK", 50) == 0)
            break;
        delay_ms(500);
    }
    
    /* 设置WiFi工作模式为Station模式 */
    esp8266_send_cmd("AT+CWMODE=1", "OK", 50);
    
    /* 软复位模块 */
    esp8266_send_cmd("AT+RST", "ready", 20);
    delay_ms(2000);  /* 等待复位完成 */
    
    /* 连接WiFi路由器（必须！超时设为15秒）*/
    esp8266_send_cmd("AT+CWJAP=\"" WIFI_NAME "\",\"" WIFI_PASSWORD "\"", "GOT IP", 1500);
    delay_ms(2000);  /* 等待网络稳定 */
    
    /* 配置MQTT用户信息（必须在MQTTCONN之前！）*/
    esp8266_send_cmd(MQTT_USERCFG, "OK", 100);
    
#if (MQTT_USE_ALIYUN == 1)
    /* 阿里云需要单独设置ClientID */
    esp8266_send_cmd(MQTT_CLIENTID, "OK", 100);
#endif
    
    /* 连接MQTT服务器 */
    esp8266_send_cmd(MQTT_CONN, "OK", 300);
}

/**
  * @brief  向ESP8266发送AT指令
  * @param  cmd: 待发送的AT指令字符串
  * @param  ack: 期望的应答字符串，为空则不等待应答
  * @param  waittime: 等待超时时间（单位：10ms）
  * @retval 0: 发送成功（收到期望应答）
  * @retval 1: 发送失败（超时未收到应答）
  */
uint8_t esp8266_send_cmd(char *cmd, char *ack, uint16_t waittime)
{
    uint8_t res = 1;  /* 默认失败 */
    
    USART2_RX_STA = 0;
    u2_printf("%s\r\n", cmd);
    
    if (ack == NULL || waittime == 0)
    {
        return 0;  /* 不需要等待应答 */
    }
    
    while (waittime--)
    {
        delay_ms(10);
        if (USART2_RX_STA & 0x8000)
        {
            if (esp8266_check_cmd(ack))
            {
                res = 0;  /* 收到期望应答，成功 */
                break;
            }
            USART2_RX_STA = 0;  /* 清除后继续等待 */
        }
    }
    
    return res;
}

/**
  * @brief  检查ESP8266应答内容
  * @param  str: 期望的应答字符串
  * @retval 0: 未找到期望字符串
  * @retval 1: 找到期望字符串
  */
uint8_t esp8266_check_cmd(char *str)
{
    char *strx = NULL;
    
    if (USART2_RX_STA & 0x8000)  /* 接收到一次数据 */
    {
        USART2_RX_BUF[USART2_RX_STA & 0x7FFF] = 0;  /* 添加字符串结束符 */
        strx = strstr((const char *)USART2_RX_BUF, (const char *)str);
    }
    
    return (strx != NULL) ? 1 : 0;
}

/**
  * @brief  向指定主题发送数据
  * @param  topic: MQTT主题
  * @param  Data: 数据值（整数）
  * @note   发送格式：AT+MQTTPUB=0,"topic","data",1,0
  */
void ESP8266_SendToTopic(const char *topic, int Data)
{
    USART2_RX_STA = 0;
    u2_printf("AT+MQTTPUB=0,\"%s\",\"%d\",1,0\r\n", topic, Data);
}

/**
  * @brief  向云端发送数据（通用，兼容旧代码）
  * @param  property: 属性名称
  * @param  Data: 属性值（整数）
  * @note   发送格式：AT+MQTTPUB=0,"topic","{\"property\":data}",1,0
  */
void ESP8266_Send(char *property, int Data)
{
    USART2_RX_STA = 0;
    u2_printf("AT+MQTTPUB=0,\"%s\",\"{\\\"%s\\\":%d}\",1,0\r\n", MQTT_TOPIC_POST, property, Data);
}

/**
  * @brief  发送ECG单点数据
  * @param  timestamp: 时间戳（未使用）
  * @param  data: ECG数据数组
  * @param  count: 数据点数
  * @note   每10ms发送1个数据点
  */
void ESP8266_SendECGBatch(uint32_t timestamp, uint16_t *data, uint8_t count)
{
    (void)timestamp;
    
    if (count == 0)
    {
        return;
    }
    
    ESP8266_SendToTopic(MQTT_TOPIC_ECG, data[0]);
}

/**
  * @brief  发送生命体征数据
  * @param  heart_rate: 心率 (bpm)
  * @param  spo2: 血氧饱和度 (%)
  * @note   JSON格式与Python服务端保持一致:
  *         {"heartRate":xx,"oxygenSaturation":xx}
  */
void ESP8266_SendVitalSign(uint16_t heart_rate, uint16_t spo2)
{
    USART2_RX_STA = 0;
    u2_printf("AT+MQTTPUB=0,\"%s\",\"{\\\"heartRate\\\":%d,\\\"oxygenSaturation\\\":%d}\",1,0\r\n",
              MQTT_TOPIC_VITAL, heart_rate, spo2);
}

/**
  * @brief  发送报警信息
  * @param  alarm_type: 报警类型 (0-4)
  * @param  severity: 严重程度 (1-5)
  * @note   JSON格式与Python服务端保持一致:
  *         {"type":x,"severity":x}
  */
void ESP8266_SendAlarm(uint8_t alarm_type, uint8_t severity)
{
    USART2_RX_STA = 0;
    u2_printf("AT+MQTTPUB=0,\"%s\",\"{\\\"type\\\":%d,\\\"severity\\\":%d}\",1,0\r\n",
              MQTT_TOPIC_ALARM, alarm_type, severity);
}

/**
  * @brief  接收云端下发的数据
  * @param  PRO: 要查找的属性名称
  * @note   解析JSON格式数据，提取属性值存入Property_Data数组
  *         
  *         示例：收到 {"Property":123}
  *         解析后 Property_Data = "123"
  */
void ESP8266_Received(char *PRO)
{
    unsigned char *ret = 0;
    char *property = 0;
    unsigned char i;
    
    if (PRO == NULL)
    {
        return;
    }
    
    if (USART2_RX_STA & 0x8000)  /* 串口2接收到一帧数据 */
    {
        ret = USART2_RX_BUF;
        if (ret != 0)
        {
            property = strstr((const char *)ret, (const char *)PRO);
            if (property != NULL)
            {
                /* 提取属性值（数字部分） */
                for (i = 0; i < 5; i++)
                {
                    if ((*(property + 13 + i) >= '0' && *(property + 13 + i) <= '9') ||
                        (*(property + 13 + i) == '}'))
                    {
                        Property_Data[i] = *(property + 13 + i);
                    }
                }
                USART2_RX_STA = 0;
            }
            else
            {
                USART2_RX_STA = 0;
            }
        }
        else
        {
            USART2_RX_STA = 0;
        }
    }
}

#endif /* USE_STDPERIPH_DRIVER */
