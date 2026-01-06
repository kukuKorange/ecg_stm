/**
  ******************************************************************************
  * @file    display.c
  * @brief   OLED显示模块实现
  * 
  * @details 封装各页面的显示功能
  ******************************************************************************
  */

#include "display.h"
#include "oled.h"
#include "ad8232.h"
#include "AD.h"

/*============================================================================*/
/*                              页面0: 心率血氧显示                            */
/*============================================================================*/

/**
 * @brief  页面0: 心率血氧显示
 */
void Display_Page0_HeartRate(void)
{
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* 标题 */
    OLED_ShowString(0, 0, "Heart Rate & SpO2", OLED_6X8);
    
    /* 分隔线 */
    OLED_DrawLine(0, 10, 127, 10);
    
    /* 心率显示 */
    OLED_ShowString(10, 16, "HR:", OLED_8X16);
    OLED_ShowNum(50, 16, data->heart_rate, 3, OLED_8X16);
    OLED_ShowString(80, 16, "bpm", OLED_8X16);
    
    /* 血氧显示 */
    OLED_ShowString(10, 36, "SpO2:", OLED_8X16);
    OLED_ShowNum(60, 36, data->spo2, 3, OLED_8X16);
    OLED_ShowString(100, 36, "%", OLED_8X16);
    
    /* 手指检测状态 */
    if (data->finger_detected)
    {
        OLED_ShowString(100, 0, "OK", OLED_6X8);
    }
    else
    {
        OLED_ShowString(100, 0, "--", OLED_6X8);
    }
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
#ifdef ENABLE_DEBUG_PAGE
    OLED_ShowString(45, 56, "1/3", OLED_6X8);
#else
    OLED_ShowString(45, 56, "1/2", OLED_6X8);
#endif
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    OLED_Update();
}

/*============================================================================*/
/*                              页面1: 心电图显示                              */
/*============================================================================*/

/**
 * @brief  页面1: 心电图显示
 */
void Display_Page1_ECG(void)
{
    /* 标题 */
    OLED_ShowString(0, 0, "ECG Monitor", OLED_6X8);
    
    /* 坐标系绘制 */
    OLED_DrawLine(1, 54, 120, 54);     /* X轴 */
    OLED_DrawLine(1, 10, 1, 54);       /* Y轴 */
    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);    /* Y轴箭头 */
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);  /* X轴箭头 */
    
    /* 显示心电数据（由Timer3中断更新） */
    OLED_ShowNum(100, 0, test, 3, OLED_6X8);
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
#ifdef ENABLE_DEBUG_PAGE
    OLED_ShowString(45, 56, "2/3", OLED_6X8);
#else
    OLED_ShowString(45, 56, "2/2", OLED_6X8);
#endif
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    OLED_Update();
}

/*============================================================================*/
/*                              页面2: 调试页面                                */
/*============================================================================*/

#ifdef ENABLE_DEBUG_PAGE
/**
 * @brief  页面2: 调试页面
 * 
 * @details 显示内容（10Hz刷新）:
 *          ┌────────────────────────┐
 *          │ [DEBUG] 10Hz           │
 *          │────────────────────────│
 *          │ Loop: 1234 10us        │
 *          │ Max:  5678 10us        │
 *          │ ADC:  2048   HR: 75    │
 *          │ Time: 123 s  SpO2: 98  │
 *          │ [<] Page 3/3     [>]   │
 *          └────────────────────────┘
 */
void Display_Page2_Debug(void)
{
    uint16_t adc_raw;
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* 读取当前ADC值 */
    adc_raw = AD_GetValue();
    
    /* 标题 */
    OLED_ShowString(0, 0, "[DEBUG] 10Hz", OLED_6X8);
    
    /* 分隔线 */
    OLED_DrawLine(0, 10, 127, 10);
    
    /* 循环时间（当前） */
    OLED_ShowString(0, 14, "Loop:", OLED_6X8);
    OLED_ShowNum(36, 14, display_loop_time_ms, 5, OLED_6X8);
    OLED_ShowString(72, 14, "10us", OLED_6X8);
    
    /* 循环时间（最大） */
    OLED_ShowString(0, 24, "Max:", OLED_6X8);
    OLED_ShowNum(30, 24, display_loop_time_max_ms, 5, OLED_6X8);
    OLED_ShowString(66, 24, "10us", OLED_6X8);
    
    /* ADC和心率 */
    OLED_ShowString(0, 34, "ADC:", OLED_6X8);
    OLED_ShowNum(30, 34, adc_raw, 4, OLED_6X8);
    OLED_ShowString(80, 34, "HR:", OLED_6X8);
    OLED_ShowNum(104, 34, data->heart_rate, 3, OLED_6X8);
    
    /* 运行时间和血氧 */
    OLED_ShowString(0, 44, "Time:", OLED_6X8);
    OLED_ShowNum(36, 44, test, 4, OLED_6X8);
    OLED_ShowString(62, 44, "s", OLED_6X8);
    OLED_ShowString(80, 44, "SpO2:", OLED_6X8);
    OLED_ShowNum(110, 44, data->spo2, 3, OLED_6X8);
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "3/3", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    OLED_Update();
}
#endif

