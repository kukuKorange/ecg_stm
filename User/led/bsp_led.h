#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/* 引脚定义 */
/*******************************************************/
/* R 红色灯 */
#define LED1_PIN                  GPIO_Pin_6               
#define LED1_GPIO_PORT            GPIOA                     
#define LED1_GPIO_CLK             RCC_APB2Periph_GPIOA

/* G 绿色灯 */
#define LED2_PIN                  GPIO_Pin_7               
#define LED2_GPIO_PORT            GPIOA                      
#define LED2_GPIO_CLK             RCC_APB2Periph_GPIOA

/* B 蓝色灯 */
#define LED3_PIN                  GPIO_Pin_3              
#define LED3_GPIO_PORT            GPIOA                      
#define LED3_GPIO_CLK             RCC_APB2Periph_GPIOA

/************************************************************/

/* 直接操作寄存器的方法控制IO */
#define digitalHi(p,i)          {p->BSRR = i;}              /* 输出为高电平 */
#define digitalLo(p,i)          {p->BRR = i;}               /* 输出低电平 */
#define digitalToggle(p,i)      {p->ODR ^= i;}              /* 输出反转状态 */

/* 带参数的宏 IO操作 */
#define LED1_TOGGLE     digitalToggle(LED1_GPIO_PORT, LED1_PIN)
#define LED1_OFF        digitalHi(LED1_GPIO_PORT, LED1_PIN)
#define LED1_ON         digitalLo(LED1_GPIO_PORT, LED1_PIN)

#define LED2_TOGGLE     digitalToggle(LED2_GPIO_PORT, LED2_PIN)
#define LED2_OFF        digitalHi(LED2_GPIO_PORT, LED2_PIN)
#define LED2_ON         digitalLo(LED2_GPIO_PORT, LED2_PIN)

#define LED3_TOGGLE     digitalToggle(LED3_GPIO_PORT, LED3_PIN)
#define LED3_OFF        digitalHi(LED3_GPIO_PORT, LED3_PIN)
#define LED3_ON         digitalLo(LED3_GPIO_PORT, LED3_PIN)

/* 基本混色，更多颜色可用PWM实现全彩颜色 */

/* 红 */
#define LED_RED  \
					LED1_ON;\
					LED2_OFF\
					LED3_OFF

/* 绿 */
#define LED_GREEN       \
					LED1_OFF;\
					LED2_ON\
					LED3_OFF

/* 蓝 */
#define LED_BLUE    \
					LED1_OFF;\
					LED2_OFF\
					LED3_ON

/* 黄(红+绿) */
#define LED_YELLOW  \
					LED1_ON;\
					LED2_ON\
					LED3_OFF

/* 紫(红+蓝) */
#define LED_PURPLE  \
					LED1_ON;\
					LED2_OFF\
					LED3_ON

/* 青(绿+蓝) */
#define LED_CYAN \
					LED1_OFF;\
					LED2_ON\
					LED3_ON
					
/* 白(红+绿+蓝) */
#define LED_WHITE   \
					LED1_ON;\
					LED2_ON\
					LED3_ON
					
/* 黑(全部关闭) */
#define LED_RGBOFF  \
					LED1_OFF;\
					LED2_OFF\
					LED3_OFF

void LED_GPIO_Config(void);

#endif /* __LED_H */
