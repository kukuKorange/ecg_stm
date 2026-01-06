/**
  ******************************************************************************
  * @file    bsp_i2c.c
  * @brief   硬件I2C驱动 - 使用STM32标准库
  ******************************************************************************
  */ 
  
#include "./i2c/bsp_i2c.h"

/* 私有变量 */
static uint32_t I2C_Timeout;

/**
  * @brief  延时函数 (毫秒)
  * @param  ms: 延时毫秒数
  */
void delay_ms(uint16_t ms)
{
    SysTick->LOAD = (SystemCoreClock / 1000) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;
    
    while (ms--) {
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    }
    SysTick->CTRL = 0;
}

/**
  * @brief  等待I2C事件
  * @param  I2C_EVENT: 等待的事件
  * @retval 0:成功, 1:超时
  */
static uint8_t I2C_WaitEvent(uint32_t I2C_EVENT)
{
    I2C_Timeout = I2C_LONG_TIMEOUT;
    while (!I2C_CheckEvent(SENSORS_I2C, I2C_EVENT)) {
        if ((I2C_Timeout--) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
  * @brief  等待I2C标志
  * @param  I2C_FLAG: 等待的标志
  * @param  Status: SET或RESET
  * @retval 0:成功, 1:超时
  */
static uint8_t I2C_WaitFlag(uint32_t I2C_FLAG, FlagStatus Status)
{
    I2C_Timeout = I2C_LONG_TIMEOUT;
    while (I2C_GetFlagStatus(SENSORS_I2C, I2C_FLAG) != Status) {
        if ((I2C_Timeout--) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
  * @brief  I2C主机初始化
*/
void I2cMaster_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    /* 使能GPIO时钟 */
    RCC_APB2PeriphClockCmd(SENSORS_I2C_SCL_GPIO_CLK | SENSORS_I2C_SDA_GPIO_CLK, ENABLE);
    
    /* 使能I2C时钟 */
    RCC_APB1PeriphClockCmd(SENSORS_I2C_CLK, ENABLE);

    /* 配置I2C引脚: SCL */
    GPIO_InitStructure.GPIO_Pin = SENSORS_I2C_SCL_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;  /* 开漏复用输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SENSORS_I2C_SCL_GPIO_PORT, &GPIO_InitStructure);

    /* 配置I2C引脚: SDA */
    GPIO_InitStructure.GPIO_Pin = SENSORS_I2C_SDA_GPIO_PIN;
    GPIO_Init(SENSORS_I2C_SDA_GPIO_PORT, &GPIO_InitStructure);
	
    /* I2C复位 */
    I2C_DeInit(SENSORS_I2C);

    /* I2C配置 */
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 400000;  /* 400kHz */

    /* 初始化I2C */
    I2C_Init(SENSORS_I2C, &I2C_InitStructure);

    /* 使能I2C */
    I2C_Cmd(SENSORS_I2C, ENABLE);
}

/**
  * @brief  I2C主机发送数据
  * @param  pdata: 数据指针
  * @param  data_size: 数据长度
  * @retval 0:成功, 非0:失败
  */
uint8_t i2c_transmit(uint8_t *pdata, uint8_t data_size)
{
    uint8_t i;

    /* 等待I2C总线空闲 */
    if (I2C_WaitFlag(I2C_FLAG_BUSY, RESET)) {
        return 1;
    }

    /* 产生起始信号 */
    I2C_GenerateSTART(SENSORS_I2C, ENABLE);

    /* 等待EV5: 起始信号已发送 */
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
        return 2;
    }

    /* 发送设备地址(写) */
    I2C_Send7bitAddress(SENSORS_I2C, I2C_WRITE_ADDR, I2C_Direction_Transmitter);

    /* 等待EV6: 地址已发送 */
    if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        return 3;
    }

    /* 发送数据 */
    for (i = 0; i < data_size; i++) {
        I2C_SendData(SENSORS_I2C, pdata[i]);
        
        /* 等待EV8: 数据已发送 */
        if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
            return 4;
        }
    }

    /* 产生停止信号 */
    I2C_GenerateSTOP(SENSORS_I2C, ENABLE);

    return 0;
}

/**
  * @brief  I2C主机接收数据
  * @param  pdata: 数据指针
  * @param  data_size: 数据长度
  * @retval 0:成功, 非0:失败
  */
uint8_t i2c_receive(uint8_t *pdata, uint8_t data_size)
{
    uint8_t i;

    /* 等待I2C总线空闲 */
    if (I2C_WaitFlag(I2C_FLAG_BUSY, RESET)) {
        return 1;
    }

    /* 产生起始信号 */
    I2C_GenerateSTART(SENSORS_I2C, ENABLE);

    /* 等待EV5 */
    if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) {
        return 2;
    }

    /* 发送设备地址(读) */
    I2C_Send7bitAddress(SENSORS_I2C, I2C_READ_ADDR, I2C_Direction_Receiver);

    /* 等待EV6 */
    if (I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) {
        return 3;
    }

    /* 使能ACK */
    I2C_AcknowledgeConfig(SENSORS_I2C, ENABLE);

    /* 接收数据 */
    for (i = 0; i < data_size; i++) {
        /* 最后一个字节前禁用ACK */
        if (i == data_size - 1) {
            I2C_AcknowledgeConfig(SENSORS_I2C, DISABLE);
        }

        /* 等待EV7: 数据已接收 */
        if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED)) {
            return 4;
        }

        /* 读取数据 */
        pdata[i] = I2C_ReceiveData(SENSORS_I2C);
    }

    /* 产生停止信号 */
    I2C_GenerateSTOP(SENSORS_I2C, ENABLE);

    return 0;
}
