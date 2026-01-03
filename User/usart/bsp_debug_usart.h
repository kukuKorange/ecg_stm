#ifndef __DEBUG_USART_H
#define __DEBUG_USART_H

#include "stm32f10x.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include <stdio.h>

/* 串口参数 */
#define DEBUG_USART_BAUDRATE                    115200

/* 引脚定义 */
/*******************************************************/
#define DEBUG_USART                             USART1
#define DEBUG_USART_CLK                         RCC_APB2Periph_USART1

#define DEBUG_USART_RX_GPIO_PORT                GPIOA
#define DEBUG_USART_RX_GPIO_CLK                 RCC_APB2Periph_GPIOA
#define DEBUG_USART_RX_PIN                      GPIO_Pin_10

#define DEBUG_USART_TX_GPIO_PORT                GPIOA
#define DEBUG_USART_TX_GPIO_CLK                 RCC_APB2Periph_GPIOA
#define DEBUG_USART_TX_PIN                      GPIO_Pin_9

#define DEBUG_USART_IRQHandler                  USART1_IRQHandler
#define DEBUG_USART_IRQn                        USART1_IRQn
/************************************************************/

void Usart_SendString(uint8_t *str);
void DEBUG_USART_Config(void);
int fputc(int ch, FILE *f);
int fgetc(FILE *f);

#endif /* __DEBUG_USART_H */
