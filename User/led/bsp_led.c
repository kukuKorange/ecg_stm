/**
 ******************************************************************************
 * @file    bsp_led.c
 * @brief   LED应用函数接口 - 使用STM32标准库
 ******************************************************************************
 */

#include "./led/bsp_led.h"
#include "max30102.h"

/**
 * @brief  初始化控制LED的IO
 * @param  无
 * @retval 无
 */
void LED_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 开启LED相关的GPIO外设时钟 */
    RCC_APB2PeriphClockCmd(LED1_GPIO_CLK | LED2_GPIO_CLK | LED3_GPIO_CLK, ENABLE);

    /* 配置LED1引脚 */
    GPIO_InitStructure.GPIO_Pin = LED1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    /* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED1_GPIO_PORT, &GPIO_InitStructure);

    /* 配置LED2引脚 */
    GPIO_InitStructure.GPIO_Pin = LED2_PIN;
    GPIO_Init(LED2_GPIO_PORT, &GPIO_InitStructure);

    /* 配置LED3引脚 */
    GPIO_InitStructure.GPIO_Pin = LED3_PIN;
    GPIO_Init(LED3_GPIO_PORT, &GPIO_InitStructure);

    /* 关闭所有LED */
    LED_RGBOFF;
}

#ifdef ENABLE_LED_INDICATOR
/**
 * @brief  LED状态更新
 * @note   根据心率血氧数据更新LED状态
 */
void LED_StatusUpdate(void)
{
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* LED1: 手指检测指示 */
    if (data->finger_detected)
    {
        LED1_ON;
    }
    else
    {
        LED1_OFF;
    }
    
    /* LED2: 心率报警 */
    if (data->heart_rate >= HR_ALARM_THRESHOLD)
    {
        LED2_ON;
    }
    else
    {
        LED2_OFF;
    }
}
#endif

/*********************************************END OF FILE**********************/
