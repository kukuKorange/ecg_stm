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
#include "OLED.h"   /* DEBUG: RST MAC 解析诊断，确认后可删除 */

/*============================ 宏定义 ============================*/

#undef USE_HAL_DRIVER  /* 禁用HAL库 */

/*============================ 全局变量 ============================*/

unsigned char Property_Data[5];             /**< 云端属性数据缓冲区           */
char esp8266_mac[18] = "--:--:--:--:--:--"; /**< STA模式MAC地址（初始化后更新）*/

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

/*============================ 私有辅助函数 ============================*/

/**
  * @brief  判断字符是否为合法十六进制字符
  */
static uint8_t prv_is_hex(char c)
{
    return ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
}

/**
  * @brief  发送 AT+RST 并在启动日志中解析 MAC 地址
  *
  * @details ESP8266 软复位后输出 boot 日志，其中一行格式为：
  *            wifi_mac:c82B961a06A1   （12位HEX，无分隔符，大小写混合）
  *
  *          【为何不能用帧模式逐行检测】：
  *          boot 日志各行背靠背以 115200bps 发出（行间无间隔），
  *          ISR 一旦检测到某行的 \r\n（帧完成，bit15=1），后续字节
  *          虽然被 ISR 读走（RXNE 照常清零），但不再写入 buffer，
  *          全部丢弃。主循环 10ms 才轮询一次，期间 wifi_mac: 所在行
  *          早已发完并被丢弃，根本无法捕获。
  *
  *          【解决方案：透传模式】：
  *          发送 AT+RST 前将 usart2_raw_mode 置 1，ISR 切换到"透传
  *          模式"——关闭 CR/LF 帧检测，将全部字节顺序累积进 buffer。
  *          等待 3 秒（boot log 含 "ready" 在 2s 内完成），再在完整
  *          日志中搜索 "wifi_mac:" 并格式化 MAC 地址。
  */
static void prv_RST_ParseMAC(void)
{
    uint16_t  acc_len;
    uint16_t  null_cnt;   /* buffer 中嵌入的 0x00 字节数量 */
    uint16_t  j;
    char     *p;
    uint8_t   i;
    char      raw_disp[13];   /* 原始12位HEX + '\0' */

    /* 切换 ISR 为透传模式，清空 buffer */
    USART2_RX_STA   = 0;
    usart2_raw_mode = 1;

    /* [DBG] 阶段1：提示 AT+RST 已发，开始计时 */
    OLED_Clear();
    OLED_ShowString(0,  0, "RST MAC Debug", OLED_6X8);
    OLED_DrawLine(0, 10, 127, 10);
    OLED_ShowString(0, 14, "AT+RST sent",   OLED_6X8);
    OLED_ShowString(0, 24, "Waiting 3s...", OLED_6X8);
    OLED_Update();

    u2_printf("AT+RST\r\n");

    /*
     * 等待 boot 日志完整输出。
     * ESP8266 boot 流程（含 ROM 乱码 + 固件日志 + ready）
     * 通常在 2s 内完成，取 3s 作为安全余量。
     */
    delay_ms(3000);

    /* 关闭透传模式，切回帧模式 */
    usart2_raw_mode = 0;

    /* null 终止累积内容（透传模式下 bit15 未置位，低14位即长度）*/
    acc_len = USART2_RX_STA & 0x3FFF;
    if (acc_len < USART2_MAX_RECV_LEN)
        USART2_RX_BUF[acc_len] = 0;
    else
        USART2_RX_BUF[USART2_MAX_RECV_LEN - 1] = 0;

    USART2_RX_STA = 0;

    /*
     * 关键修复：ROM 在 74880bps 输出，被 115200bps 接收后产生乱码，
     * 乱码字节中可能含有 0x00（NULL）。strstr() 遇 NULL 即停止，
     * 会在 wifi_mac: 出现之前提前终止搜索，导致永远找不到。
     * 解决方法：扫描一遍，将所有嵌入的 0x00 替换为 0xFF。
     */
    null_cnt = 0;
    for (j = 0; j < acc_len; j++)
    {
        if (USART2_RX_BUF[j] == 0x00)
        {
            USART2_RX_BUF[j] = 0xFF;
            null_cnt++;
        }
    }

    /* [DBG] 阶段2：显示收到字节数及发现的 NULL 数量 */
    OLED_Clear();
    OLED_ShowString(0,  0, "RST MAC Debug",  OLED_6X8);
    OLED_DrawLine(0, 10, 127, 10);
    OLED_ShowString(0, 14, "Rx:",  OLED_6X8);
    OLED_ShowNum(20, 14, acc_len,  4, OLED_6X8);  /* 收到字节数 */
    OLED_ShowString(66, 14, "NUL:", OLED_6X8);
    OLED_ShowNum(90, 14, null_cnt, 3, OLED_6X8);  /* NULL 字节数（预期 > 0）*/
    OLED_ShowString(0, 24, "Searching...",   OLED_6X8);
    OLED_Update();
    delay_ms(1500);

    /* 在完整 boot 日志中搜索 wifi_mac: */
    p = strstr((char *)USART2_RX_BUF, "wifi_mac:");
    if (p != NULL)
    {
        p += 9;  /* 跳过 "wifi_mac:" */

        /* 校验后续 12 个字符必须全为十六进制 */
        for (i = 0; i < 12; i++)
        {
            if (!prv_is_hex(p[i]))
                break;
        }

        /* 保存原始12字符用于显示 */
        for (j = 0; j < 12; j++)
            raw_disp[j] = p[j];
        raw_disp[12] = '\0';

        /* [DBG] 阶段3a：找到关键字，显示原始HEX及校验结果 */
        OLED_Clear();
        OLED_ShowString(0,  0, "RST MAC Debug",  OLED_6X8);
        OLED_DrawLine(0, 10, 127, 10);
        OLED_ShowString(0, 14, "FOUND!",          OLED_6X8);
        OLED_ShowString(0, 24, "Raw:", OLED_6X8);
        OLED_ShowString(30, 24, raw_disp,          OLED_6X8);  /* "c82B961a06A1" */
        OLED_ShowString(0, 34, "HexOK:",           OLED_6X8);
        OLED_ShowNum(42, 34, i, 2, OLED_6X8);                  /* 应为 12 */
        OLED_ShowString(54, 34, "/12",             OLED_6X8);
        OLED_Update();
        delay_ms(2000);

        if (i == 12)
        {
            /*
             * 将 "c82B961a06A1" 转换为 "c8:2b:96:1a:06:a1"
             * 对任意十六进制字符 c：c | 0x20 等价于 tolower(c)
             */
            for (i = 0; i < 6; i++)
            {
                esp8266_mac[i * 3]     = p[i * 2]     | 0x20;
                esp8266_mac[i * 3 + 1] = p[i * 2 + 1] | 0x20;
                esp8266_mac[i * 3 + 2] = (i < 5) ? ':' : '\0';
            }
            esp8266_mac[17] = '\0';

            /* [DBG] 阶段4：成功，显示最终格式化 MAC */
            OLED_Clear();
            OLED_ShowString(0,  0, "RST MAC Debug", OLED_6X8);
            OLED_DrawLine(0, 10, 127, 10);
            OLED_ShowString(0, 14, "MAC OK!",        OLED_6X8);
            OLED_ShowString(0, 26, esp8266_mac,      OLED_6X8);
            OLED_Update();
            delay_ms(2000);
        }
    }
    else
    {
        /*
         * [DBG] 阶段3b：替换 NULL 后仍未找到。
         * 此时 wifi_mac: 可能位于 buffer 满（599字节）之后，
         * 即 ROM 乱码超过 ~400 字节，将 USART2_MAX_RECV_LEN 增大可解决。
         */
        OLED_Clear();
        OLED_ShowString(0,  0, "RST MAC Debug",  OLED_6X8);
        OLED_DrawLine(0, 10, 127, 10);
        OLED_ShowString(0, 14, "NOT FOUND!",      OLED_6X8);
        OLED_ShowString(0, 24, "Rx:",             OLED_6X8);
        OLED_ShowNum(20, 24, acc_len, 4, OLED_6X8);
        OLED_ShowString(66, 24, "NUL:",           OLED_6X8);
        OLED_ShowNum(90, 24, null_cnt, 3, OLED_6X8);
        OLED_ShowString(0, 34, "->IncMAX_LEN",    OLED_6X8);  /* 提示解决方案 */
        OLED_Update();
        delay_ms(3000);
    }
}

/*============================ 公共函数 ============================*/

/**
  * @brief  ESP8266模块初始化
  * @note   初始化流程：
  *         1. 测试AT通信
  *         2. 设置Station模式
  *         3. AT+RST（同时解析boot日志中的MAC地址）
  *         4. 连接WiFi路由器
  *         5. 配置MQTT并连接服务器
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
    
    /* 软复位模块，同步解析 boot 日志中的 wifi_mac 字段 */
    prv_RST_ParseMAC();
    delay_ms(2000);  /* 等待网络接口就绪 */
    
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
