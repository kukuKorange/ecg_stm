/**
  ******************************************************************************
  * @file    main.h
  * @brief   主程序头文件 - 使用STM32标准库
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* 全局配置文件 - 必须最先包含 */
#include "kconfig.h"

/* 禁用HAL库，使用标准外设库 */
#undef USE_HAL_DRIVER

/* 启用标准外设库 */
#ifndef USE_STDPERIPH_DRIVER
#define USE_STDPERIPH_DRIVER
#endif

#ifndef STM32F10X_MD
#define STM32F10X_MD
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_exti.h"
#include "misc.h"

/* Exported functions ------------------------------------------------------- */
void SystemClock_Config(void);
void Error_Handler(void);

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
