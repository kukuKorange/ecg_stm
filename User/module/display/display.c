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
#include "Key.h"

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint8_t last_page = 0xFF;          /**< 上一次的页面，用于检测页面切换 */

/* 页面0局部刷新相关 */
static uint8_t  page0_static_drawn = 0;   /**< 页面0静态内容是否已绘制 */
static uint16_t last_hr = 0xFFFF;         /**< 上次心率值 */
static uint16_t last_spo2 = 0xFFFF;       /**< 上次血氧值 */
static uint8_t  last_finger = 0xFF;       /**< 上次手指检测状态 */

/*============================================================================*/
/*                              显示更新（主入口）                              */
/*============================================================================*/

/**
 * @brief  显示更新
 * @note   处理页面切换检测并调用对应页面显示函数
 */
void Display_Update(void)
{
    /* 页面切换检测 */
    if (current_page != last_page)
    {
        OLED_Clear();
        last_page = current_page;
        page0_static_drawn = 0;  /* 重置页面0静态内容标志 */
#ifdef ENABLE_DEBUG_PAGE
        extern uint32_t display_loop_time_max_ms;
        display_loop_time_max_ms = 0;  /* 切换页面时重置最大时间 */
#endif
    }
    
    /* 根据当前页面显示内容 */
    switch (current_page)
    {
        case PAGE_HEARTRATE:
            /* 心率页面：10Hz刷新 */
            if (display_refresh_flag)
            {
                display_refresh_flag = 0;
                Display_Page0_HeartRate();
            }
            break;
            
        case PAGE_ECG:
            Display_Page1_ECG();
            break;
            
#ifdef ENABLE_DEBUG_PAGE
        case PAGE_DEBUG:
            /* 调试页面：10Hz刷新 */
            if (debug_refresh_flag)
            {
                debug_refresh_flag = 0;
                Display_Page2_Debug();
            }
            break;
#endif
            
        default:
            current_page = PAGE_HEARTRATE;
            break;
    }
}

/*============================================================================*/
/*                              页面0: 心率血氧显示                            */
/*============================================================================*/

/**
 * @brief  页面0: 绘制静态内容（仅在页面切换时调用一次）
 */
static void Display_Page0_DrawStatic(void)
{
    /* 标题 */
    OLED_ShowString(0, 0, "Heart Rate & SpO2", OLED_6X8);
    
    /* 分隔线 */
    OLED_DrawLine(0, 10, 127, 10);
    
    /* 心率标签 */
    OLED_ShowString(10, 16, "HR:", OLED_8X16);
    OLED_ShowString(80, 16, "bpm", OLED_8X16);
    
    /* 血氧标签 */
    OLED_ShowString(10, 36, "SpO2:", OLED_8X16);
    OLED_ShowString(100, 36, "%", OLED_8X16);
    
    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
#ifdef ENABLE_DEBUG_PAGE
    OLED_ShowString(45, 56, "1/3", OLED_6X8);
#else
    OLED_ShowString(45, 56, "1/2", OLED_6X8);
#endif
    OLED_ShowString(110, 56, "K3>", OLED_6X8);
    
    page0_static_drawn = 1;
}

/**
 * @brief  页面0: 心率血氧显示（局部刷新优化）
 * @note   仅更新变化的数值区域，使用 OLED_UpdateArea 局部刷新
 * 
 * @details 刷新区域定义:
 *          - 心率数值: X=50, Y=16, 宽24(3字*8), 高16 (8x16字体)
 *          - 血氧数值: X=60, Y=36, 宽24(3字*8), 高16 (8x16字体)
 *          - 手指状态: X=100, Y=0, 宽12(2字*6), 高8 (6x8字体)
 */
void Display_Page0_HeartRate(void)
{
    MAX30102_Data_t *data = MAX30102_GetData();
    
    /* 首次进入页面，绘制静态内容并全屏刷新 */
    if (!page0_static_drawn)
    {
        Display_Page0_DrawStatic();
        last_hr = 0xFFFF;      /* 强制刷新数值 */
        last_spo2 = 0xFFFF;
        last_finger = 0xFF;
        
        /* 绘制初始数值 */
        OLED_ShowNum(50, 16, data->heart_rate, 3, OLED_8X16);
        OLED_ShowNum(60, 36, data->spo2, 3, OLED_8X16);
        OLED_ShowString(100, 0, data->finger_detected ? "OK" : "--", OLED_6X8);
        
        /* 首次全屏刷新 */
        OLED_Update();
        
        last_hr = data->heart_rate;
        last_spo2 = data->spo2;
        last_finger = data->finger_detected;
        return;
    }
    
    /* 心率值变化时局部刷新 */
    if (data->heart_rate != last_hr)
    {
        last_hr = data->heart_rate;
        OLED_ShowNum(50, 16, data->heart_rate, 3, OLED_8X16);
        OLED_UpdateArea(50, 16, 24, 16);  /* 只刷新心率数字区域 */
    }
    
    /* 血氧值变化时局部刷新 */
    if (data->spo2 != last_spo2)
    {
        last_spo2 = data->spo2;
        OLED_ShowNum(60, 36, data->spo2, 3, OLED_8X16);
        OLED_UpdateArea(60, 36, 24, 16);  /* 只刷新血氧数字区域 */
    }
    
    /* 手指检测状态变化时局部刷新 */
    if (data->finger_detected != last_finger)
    {
        last_finger = data->finger_detected;
        OLED_ShowString(100, 0, data->finger_detected ? "OK" : "--", OLED_6X8);
        OLED_UpdateArea(100, 0, 12, 8);   /* 只刷新状态区域 */
    }
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

