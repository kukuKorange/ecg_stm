#ifndef __MAX30102_H
#define __MAX30102_H

#include "stm32f10x.h"
#include <stdint.h>
#include "kconfig.h"

/*============================================================================*/
/*                              数据结构定义                                   */
/*============================================================================*/

/**
 * @brief  心率血氧数据结构体
 * @note   用于存储MAX30102模块计算得到的心率和血氧数据
 */
typedef struct {
    uint16_t heart_rate;        /**< 心率值 (bpm) */
    uint16_t spo2;              /**< 血氧值 (%) */
    uint8_t  finger_detected;   /**< 手指检测标志: 1=检测到, 0=未检测到 */
    uint8_t  data_ready;        /**< 数据就绪标志: 1=新数据可用, 0=无新数据 */
} MAX30102_Data_t;

/**
 * @brief  全局心率血氧数据（供其他模块使用）
 */
extern MAX30102_Data_t g_max30102_data;

/*============================================================================*/
/*                              引脚定义                                       */
/*============================================================================*/

/* MAX30102中断引脚定义 */
#define MAX30102_INT_Pin            GPIO_Pin_5
#define MAX30102_INT_GPIO_Port      GPIOB
#define MAX30102_INT_GPIO_CLK       RCC_APB2Periph_GPIOB
#define MAX30102_INT_EXTI_Line      EXTI_Line5
#define MAX30102_INT_EXTI_PortSource GPIO_PortSourceGPIOB
#define MAX30102_INT_EXTI_PinSource GPIO_PinSource5
#define MAX30102_INT_EXTI_IRQn      EXTI9_5_IRQn

/* I2C地址 */
#define I2C_WRITE_ADDR 0xAE
#define I2C_READ_ADDR  0xAF

/* MAX30102寄存器地址 */
#define INTERRUPT_STATUS1    0X00
#define INTERRUPT_STATUS2    0X01
#define INTERRUPT_ENABLE1    0X02
#define INTERRUPT_ENABLE2    0X03

#define FIFO_WR_POINTER      0X04
#define FIFO_OV_COUNTER      0X05
#define FIFO_RD_POINTER      0X06
#define FIFO_DATA            0X07

#define FIFO_CONFIGURATION   0X08
#define MODE_CONFIGURATION   0X09
#define SPO2_CONFIGURATION   0X0A
#define LED1_PULSE_AMPLITUDE 0X0C
#define LED2_PULSE_AMPLITUDE 0X0D

#define MULTILED1_MODE       0X11
#define MULTILED2_MODE       0X12

#define TEMPERATURE_INTEGER  0X1F
#define TEMPERATURE_FRACTION 0X20
#define TEMPERATURE_CONFIG   0X21

#define VERSION_ID           0XFE
#define PART_ID              0XFF

/*============================================================================*/
/*                              函数声明                                       */
/*============================================================================*/

/* 初始化函数 */
void max30102_int_gpio_init(void);
void max30102_init(void);

/* 底层读写函数 */
void max30102_fifo_read(float *data);
void max30102_i2c_read(uint8_t reg_adder, uint8_t *pdata, uint8_t data_size);

/* 数据处理函数（内部使用） */
uint16_t max30102_getHeartRate(float *input_data, uint16_t cache_nums);
float max30102_getSpO2(float *ir_input_data, float *red_input_data, uint16_t cache_nums);

/**
 * @brief  心率血氧数据处理（主循环调用）
 * @note   此函数完成以下工作:
 *         1. 读取FIFO数据
 *         2. FIR滤波
 *         3. 数据缓存
 *         4. 计算心率和血氧
 *         5. 更新 g_max30102_data 结构体
 */
void MAX30102_Process(void);

/**
 * @brief  获取心率血氧数据指针
 * @retval 指向 MAX30102_Data_t 结构体的指针
 */
MAX30102_Data_t* MAX30102_GetData(void);

#endif /* __MAX30102_H */
