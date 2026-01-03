#ifndef _BSP_I2C_H
#define _BSP_I2C_H

#include <inttypes.h>
#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/* MAX30102 I2C地址 */
#define I2C_WRITE_ADDR 0xAE
#define I2C_READ_ADDR  0xAF

/* I2C外设定义 */
#define SENSORS_I2C              I2C1
#define SENSORS_I2C_CLK          RCC_APB1Periph_I2C1

/* I2C引脚定义 */
#define SENSORS_I2C_SCL_GPIO_PORT   GPIOB
#define SENSORS_I2C_SCL_GPIO_CLK    RCC_APB2Periph_GPIOB
#define SENSORS_I2C_SCL_GPIO_PIN    GPIO_Pin_6

#define SENSORS_I2C_SDA_GPIO_PORT   GPIOB
#define SENSORS_I2C_SDA_GPIO_CLK    RCC_APB2Periph_GPIOB
#define SENSORS_I2C_SDA_GPIO_PIN    GPIO_Pin_7

/* I2C超时时间 */
#define I2C_TIMEOUT             ((uint32_t)0x1000)
#define I2C_LONG_TIMEOUT        ((uint32_t)(10 * I2C_TIMEOUT))

/* 函数声明 */
void I2cMaster_Init(void);
uint8_t i2c_transmit(uint8_t *pdata, uint8_t data_size);
uint8_t i2c_receive(uint8_t *pdata, uint8_t data_size);
void delay_ms(uint16_t ms);

#endif
