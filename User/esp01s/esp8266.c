/**
  ******************************************************************************
  * @file    esp8266.c
  * @brief   ESP8266 WiFi模块驱动 (ESP-01S)
  * 
  * @details 通过AT指令控制ESP8266模块，实现：
  *          - WiFi Station模式连接路由器
  *          - MQTT协议连接阿里云IoT平台
  *          - 传感器数据上传与云端指令接收
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "esp8266.h"
#include "usart2.h"
#include "string.h"
#include "stdint.h"

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
  *         1. 设置Station模式（连接WiFi）
  *         2. 软复位模块
  *         3. 连接WiFi路由器
  *         4. 配置MQTT连接参数
  */
void ESP8266_Init(void)
{
    /* 设置WiFi工作模式为Station模式 */
    esp8266_send_cmd("AT+CWMODE=1", "OK", 50);
    
    /* 软复位模块 */
    esp8266_send_cmd("AT+RST", "ready", 20);
    delay_ms(1000);
    
    /* 连接WiFi路由器 (SSID: 5132, 密码: 53288837) */
    esp8266_send_cmd("AT+CWJAP=\"5132\",\"53288837\"", "WIFI GOT IP", 300);
    
    /* 配置MQTT用户信息 */
    esp8266_send_cmd(MQTT_set, "OK", 100);
    
    /* 配置MQTT客户端ID */
    esp8266_send_cmd(MQTT_Client, "OK", 100);
    
    /* 连接MQTT服务器 */
    esp8266_send_cmd(MQTT_Pass, "OK", 300);
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
    uint8_t res = 0;
    
    USART2_RX_STA = 0;
    u2_printf("%s\r\n", cmd);
    
    if (waittime)
    {
        while (--waittime)
        {
            delay_ms(10);
            if (USART2_RX_STA & 0x8000)
            {
                /* TODO: 启用应答检查 */
                // if (esp8266_check_cmd(ack))
                // {
                //     break;
                // }
                USART2_RX_STA = 0;
            }
        }
        if (waittime == 0)
        {
            res = 1;
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
  * @brief  向云端发送数据
  * @param  property: 属性名称
  * @param  Data: 属性值（整数）
  * @note   发送格式：AT+MQTTPUB=0,"topic","{\"property\":data}",1,0
  */
void ESP8266_Send(char *property, int Data)
{
    USART2_RX_STA = 0;
    u2_printf("AT+MQTTPUB=0,\"%s\",\"{\\\"%s\\\":%d}\",1,0\r\n", post, property, Data);
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
