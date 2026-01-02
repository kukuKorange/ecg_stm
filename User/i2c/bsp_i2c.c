/**
  ******************************************************************************
  * @file    bsp_i2c.c
  * @version V1.0
  * @date    2013-xx-xx
  * @brief   硬件I2C
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火 F103-MINI STM32 开发板 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */ 
  
#include "./i2c/bsp_i2c.h"
#include "stm32f1xx.h"
#include "main.h"

/*
*********************************************************************************************************
*	函 数 名: i2c_CfgGpio
*	功能说明: 配置I2C总线的GPIO，采用硬件I2C的方式实现
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2cMaster_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 使能I2Cx时钟 */
	SENSORS_I2C_RCC_CLK_ENABLE();

	/* 使能I2C GPIO 时钟 */
	SENSORS_I2C_SCL_GPIO_CLK_ENABLE();
	SENSORS_I2C_SDA_GPIO_CLK_ENABLE();

	/* 配置I2Cx引脚: SCL ----------------------------------------*/
	GPIO_InitStructure.Pin =  SENSORS_I2C_SCL_GPIO_PIN; 
	GPIO_InitStructure.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStructure.Pull= GPIO_NOPULL;
//	GPIO_InitStructure.Alternate=SENSORS_I2C_AF; 
	HAL_GPIO_Init(SENSORS_I2C_SCL_GPIO_PORT, &GPIO_InitStructure);

	/* 配置I2Cx引脚: SDA ----------------------------------------*/
	GPIO_InitStructure.Pin = SENSORS_I2C_SDA_GPIO_PIN;  
	HAL_GPIO_Init(SENSORS_I2C_SDA_GPIO_PORT, &GPIO_InitStructure); 
	
	if(HAL_I2C_GetState(&I2C_Handle) == HAL_I2C_STATE_RESET)
	{	
		/* 强制复位I2C外设时钟 */  
		SENSORS_I2C_FORCE_RESET(); 

		/* 释放I2C外设时钟复位 */  
		SENSORS_I2C_RELEASE_RESET(); 
		
		/* I2C 配置 */
        I2C_Handle.Instance = SENSORS_I2C;
        I2C_Handle.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
        I2C_Handle.Init.ClockSpeed      = 400000;
        I2C_Handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
        I2C_Handle.Init.DutyCycle       = I2C_DUTYCYCLE_2;
        I2C_Handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
        I2C_Handle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
        I2C_Handle.Init.OwnAddress1     = 0;
        I2C_Handle.Init.OwnAddress2     = 0;     
		/* 初始化I2C */
		HAL_I2C_Init(&I2C_Handle);

    }

}

