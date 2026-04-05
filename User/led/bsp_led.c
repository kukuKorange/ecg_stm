/**
 ******************************************************************************
 * @file    bsp_led.c
 * @brief   LED应用函数接口 - 使用STM32标准库
 ******************************************************************************
 */

#include "./led/bsp_led.h"
#ifdef USE_ECG_SIM
#include "ecg_sim/ecg_sim.h"
#endif
#include "kconfig.h"
#ifdef USE_MAX30102
#include "max30102.h"
#else
#include "ad8232.h"
#endif

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
 * @note   根据心率数据更新LED状态
 */
void LED_StatusUpdate(void)
{
#ifdef USE_MAX30102
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
    if (data->heart_rate >= HR_ALARM_THRESHOLD_HIGH || data->heart_rate <= HR_ALARM_THRESHOLD_LOW)
    {
        LED2_ON;
    }
    else
    {
        LED2_OFF;
    }
#else
    /* 无MAX30102：LED1关闭（无手指检测），LED2基于ECG心率报警 */
    LED1_OFF;
#ifdef USE_ECG_SIM
    uint8_t ecg_hr = ECG_Sim_GetBPM();
#else
    uint8_t ecg_hr = ECG_GetHeartRate();
#endif
    if (ecg_hr >= HR_ALARM_THRESHOLD_HIGH || ecg_hr <= HR_ALARM_THRESHOLD_LOW)
    {
        LED1_ON;
    }
    else
    {
        LED1_OFF;
    }
#endif
}
#endif

/*********************************************END OF FILE**********************/
