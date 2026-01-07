/**
  ******************************************************************************
  * @file    usart2.c
  * @brief   USART2串口驱动 (用于ESP8266通信)
  * 
  * @details 实现功能：
  *          - USART2初始化 (PA2-TX, PA3-RX)
  *          - 中断接收（帧结束标志: 0x0D 0x0A）
  *          - printf风格的格式化发送
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "usart2.h"
#include "sys.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

/*============================ 全局变量 ============================*/

/** @brief 发送缓冲区 (8字节对齐) */
__align(8) uint8_t USART2_TX_BUF[USART2_MAX_SEND_LEN];

/** @brief 接收缓冲区 */
uint8_t USART2_RX_BUF[USART2_MAX_RECV_LEN];

/**
  * @brief  接收状态标志
  * @note   帧模式接收，以 0x0D 0x0A (\\r\\n) 作为帧结束标志
  *         
  *         bit15:    接收完成标志 (1=收到完整帧)
  *         bit14:    收到0x0D标志
  *         bit[13:0]: 已接收数据长度
  */
uint16_t USART2_RX_STA = 0;

/*============================ 函数实现 ============================*/

/**
  * @brief  USART2初始化
  * @param  bound: 波特率
  * @note   引脚配置：
  *         - PA2: USART2_TX (复用推挽输出)
  *         - PA3: USART2_RX (浮空输入)
  */
void usart2_init(uint32_t bound)
{
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* 配置 PA2 (TX) 为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 配置 PA3 (RX) 为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 配置中断优先级 */
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;  /* 抢占优先级3 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;         /* 子优先级3 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 配置USART2参数 */
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  /* 使能接收中断 */
    USART_Cmd(USART2, ENABLE);
}

#ifdef USART2_RX_EN
/**
  * @brief  USART2中断服务函数
  * @note   帧接收逻辑：
  *         - 接收数据直到遇到 0x0D (\\r)
  *         - 下一个字节为 0x0A (\\n) 时，标记接收完成
  *         - 接收完成后 USART2_RX_STA 的 bit15 置1
  */
void USART2_IRQHandler(void)
{
    uint8_t Res;

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        Res = USART_ReceiveData(USART2);

        if ((USART2_RX_STA & 0x8000) == 0)  /* 接收未完成 */
        {
            if (USART2_RX_STA & 0x4000)     /* 已收到 0x0D */
            {
                if (Res != 0x0A)
                {
                    USART2_RX_STA = 0;      /* 接收错误，重新开始 */
                }
                else
                {
                    USART2_RX_STA |= 0x8000; /* 接收完成 */
                }
            }
            else  /* 未收到 0x0D */
            {
                if (Res == 0x0D)
                {
                    USART2_RX_STA |= 0x4000; /* 标记收到 0x0D */
                }
                else
                {
                    USART2_RX_BUF[USART2_RX_STA & 0x3FFF] = Res;
                    USART2_RX_STA++;

                    /* 溢出保护 */
                    if (USART2_RX_STA > (USART2_MAX_RECV_LEN - 1))
                    {
                        USART2_RX_STA = 0;
                    }
                }
            }
        }
    }
}
#endif /* USART2_RX_EN */

/**
  * @brief  USART2格式化发送函数
  * @param  fmt: 格式化字符串
  * @param  ...: 可变参数
  * @note   使用方法与printf相同，最大发送长度为 USART2_MAX_SEND_LEN
  * 
  * @example u2_printf("AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
  */
void u2_printf(char *fmt, ...)
{
    uint16_t i, j;
    va_list ap;

    va_start(ap, fmt);
    vsprintf((char *)USART2_TX_BUF, fmt, ap);
    va_end(ap);

    i = strlen((const char *)USART2_TX_BUF);

    for (j = 0; j < i; j++)
    {
        /* 等待上次发送完成 */
        while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
        USART_SendData(USART2, (uint8_t)USART2_TX_BUF[j]);
    }
}

#endif /* USE_STDPERIPH_DRIVER */
