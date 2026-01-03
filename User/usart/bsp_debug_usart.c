/**
 ******************************************************************************
 * @file    bsp_debug_usart.c
 * @brief   调试串口驱动 - 使用STM32标准库
 ******************************************************************************
 */

#include "./usart/bsp_debug_usart.h"

/**
 * @brief  DEBUG_USART GPIO 配置，工作模式配置 115200 8-N-1
 * @param  无
 * @retval 无
 */
void DEBUG_USART_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    /* 使能GPIO时钟 */
    RCC_APB2PeriphClockCmd(DEBUG_USART_TX_GPIO_CLK | DEBUG_USART_RX_GPIO_CLK, ENABLE);
    
    /* 使能USART1时钟 */
    RCC_APB2PeriphClockCmd(DEBUG_USART_CLK, ENABLE);

    /* 配置USART1 Tx引脚为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = DEBUG_USART_TX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DEBUG_USART_TX_GPIO_PORT, &GPIO_InitStructure);

    /* 配置USART1 Rx引脚为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = DEBUG_USART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DEBUG_USART_RX_GPIO_PORT, &GPIO_InitStructure);

    /* USART1配置 */
    USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    
    USART_Init(DEBUG_USART, &USART_InitStructure);
    
    /* 使能USART1 */
    USART_Cmd(DEBUG_USART, ENABLE);
}

/*****************  发送字符串 **********************/
void Usart_SendString(uint8_t *str)
{
    unsigned int k = 0;
    do
    {
        /* 等待发送缓冲区空 */
        while(USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
        USART_SendData(DEBUG_USART, *(str + k));
        k++;
    } while (*(str + k) != '\0');
    
    /* 等待发送完成 */
    while(USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TC) == RESET);
}

/* 重定向c库函数printf到DEBUG_USART */
int fputc(int ch, FILE *f)
{
    /* 等待发送缓冲区空 */
    while(USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
    
    /* 发送一个字节数据到串口DEBUG_USART */
    USART_SendData(DEBUG_USART, (uint8_t)ch);

    return (ch);
}

/* 重定向c库函数scanf到DEBUG_USART */
int fgetc(FILE *f)
{
    /* 等待接收缓冲区非空 */
    while(USART_GetFlagStatus(DEBUG_USART, USART_FLAG_RXNE) == RESET);
    
    return (int)USART_ReceiveData(DEBUG_USART);
}

/*********************************************END OF FILE**********************/
