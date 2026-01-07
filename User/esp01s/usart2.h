/**
  ******************************************************************************
  * @file    usart2.h
  * @brief   USART2串口驱动头文件 (用于ESP8266通信)
  ******************************************************************************
  */

#ifndef __USART2_H
#define __USART2_H

#ifdef USE_STDPERIPH_DRIVER

#include "stdint.h"
#include "stdio.h"

/*============================ 配置宏 ============================*/

#define USART2_MAX_RECV_LEN     600     /**< 最大接收缓冲区大小 (字节) */
#define USART2_MAX_SEND_LEN     600     /**< 最大发送缓冲区大小 (字节) */
#define USART2_RX_EN            1       /**< 接收使能: 0=禁用, 1=启用 */

/*============================ 外部变量 ============================*/

extern uint8_t  USART2_RX_BUF[USART2_MAX_RECV_LEN];  /**< 接收缓冲区 */
extern uint16_t USART2_RX_STA;                        /**< 接收状态标志 */

/*============================ 函数声明 ============================*/

/**
  * @brief  USART2初始化
  * @param  bound: 波特率 (如 115200)
  */
void usart2_init(uint32_t bound);

/**
  * @brief  USART2格式化发送 (类似printf)
  * @param  fmt: 格式化字符串
  * @param  ...: 可变参数
  * 
  * @example u2_printf("AT+RST\r\n");
  * @example u2_printf("Value: %d\r\n", value);
  */
void u2_printf(char *fmt, ...);

#endif /* USE_STDPERIPH_DRIVER */

#endif /* __USART2_H */
