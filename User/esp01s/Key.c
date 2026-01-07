/**
  ******************************************************************************
  * @file    Key.c
  * @brief   按键驱动 - 实现翻页功能
  * 
  * @details 按键功能:
  *          - Key1 (PB12): 上一页
  *          - Key2 (PB13): 功能键（上传心电数据）
  *          - Key3 (PB14): 下一页
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

#include "key.h"
#include "Timer2.h"
#include "esp8266.h"
#include "oled.h"

/*============================ 全局变量 ============================*/

uint8_t current_page = PAGE_HEARTRATE;  /**< 当前页面，默认心率页面 */
/*============================ 函数实现 ============================*/

/**
  * @brief  按键初始化
  * @param  无
  * @retval 无
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* 开启GPIOB时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    /* GPIO初始化: PB12, PB13, PB14 配置为上拉输入 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  按键扫描获取键码
  * @param  无
  * @retval 按下按键的键码值: 0=无按键, 1=Key1, 2=Key2, 3=Key3
  * @note   此函数是阻塞式操作，按键按住不放时会等待松手
  */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;
    
    /* Key1: PB12 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0)
    {
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0);  /* 等待松手 */
        KeyNum = 1;
    }
    
    /* Key2: PB13 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0)
    {
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == 0);  /* 等待松手 */
        KeyNum = 2;
    }
    
    /* Key3: PB14 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0)
    {
        while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) == 0);  /* 等待松手 */
        KeyNum = 3;
    }
    
    return KeyNum;
}

/**
  * @brief  按键处理函数 - 实现翻页逻辑
  * @param  无
  * @retval 无
  * 
  * @details 按键功能分配:
  *          ┌─────────────────────────────────────┐
  *          │  [Key1]      [Key2]      [Key3]     │
  *          │  上一页      功能键       下一页     │
  *          └─────────────────────────────────────┘
  */
void Key_Process(void)
{
    uint8_t KeyNum = Key_GetNum();
    
    if (KeyNum == 0) return;  /* 无按键按下 */
    
    switch (KeyNum)
    {
        case 1:  /* Key1: 上一页 */
            if (current_page > 0)
            {
                current_page--;
            }
            else
            {
                current_page = PAGE_MAX - 1;  /* 循环到最后一页 */
            }
            OLED_Clear();  /* 清屏准备显示新页面 */
            break;
            
        case 2:  /* Key2: 功能键 - 上传心电数据 */
            if (current_page == PAGE_ECG)
            {
                TIM_Cmd(TIM3, DISABLE);
                uint8_t cnt;
                for (cnt = 0; cnt < 120; cnt++)
                {
                    // ESP8266_Send("HeartMap", 80 - map_upload[cnt]);
                }
                TIM_Cmd(TIM3, ENABLE);
            }
            break;
            
        case 3:  /* Key3: 下一页 */
            if (current_page < PAGE_MAX - 1)
            {
                current_page++;
            }
            else
            {
                current_page = 0;  /* 循环到第一页 */
            }
            OLED_Clear();  /* 清屏准备显示新页面 */
            break;
            
        default:
            break;
    }
}

#endif
