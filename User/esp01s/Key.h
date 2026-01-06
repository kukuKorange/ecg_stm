/**
  ******************************************************************************
  * @file    Key.h
  * @brief   按键驱动头文件
  ******************************************************************************
  */

#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>
#include "kconfig.h"  /* 全局配置（包含 ENABLE_DEBUG_PAGE 等宏定义） */

/*============================ 页面定义 ============================*/

#define PAGE_HEARTRATE    0     /**< 页面0: 心率血氧显示 */
#define PAGE_ECG          1     /**< 页面1: 心电图显示 */

#ifdef ENABLE_DEBUG_PAGE
#define PAGE_DEBUG        2     /**< 页面2: 调试页面 */
#define PAGE_MAX          3     /**< 总页面数（含调试页面）*/
#else
#define PAGE_MAX          2     /**< 总页面数（不含调试页面）*/
#endif

/*============================ 外部变量 ============================*/

extern uint8_t current_page;    /**< 当前页面索引 */

/*============================ 函数声明 ============================*/

void Key_Init(void);
uint8_t Key_GetNum(void);
void Key_Process(void);

#endif
