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
#include "esp8266.h"
#ifdef USE_ECG_SIM
#include "ecg_sim.h"
#endif
#ifdef USE_DS18B20
#include "ds18b20/ds18b20.h"
#endif

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint8_t last_page = 0xFF;          /**< 上一次的页面，用于检测页面切换 */

/* 页面0局部刷新相关 */
static uint8_t  page0_static_drawn = 0;   /**< 页面0静态内容是否已绘制 */
static uint16_t last_hr     = 0xFFFF;     /**< 上次检测心率值 */
static uint8_t  last_sim_bpm = 0xFF;      /**< 上次模拟器BPM（用于局部刷新）*/
#ifdef USE_MAX30102
static uint16_t last_spo2 = 0xFFFF;       /**< 上次血氧值 */
static uint8_t  last_finger = 0xFF;       /**< 上次手指检测状态 */
#endif
#ifdef USE_DS18B20
static float last_temp = -127.0f;         /**< 上次温度值 */
static uint8_t last_temp_valid = 0;       /**< 上次温度有效标志 */
#endif


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
#ifdef USE_MAX30102
    /* 标题: 心率+血氧 */
    OLED_ShowString(0, 0, "Heart Rate & SpO2", OLED_6X8);
#else
    /* 标题: 仅ECG心率 */
    OLED_ShowString(0, 0, "Heart Rate (ECG)", OLED_6X8);
#endif

    /* 分隔线 */
    OLED_DrawLine(0, 10, 127, 10);
    
    /* 心率标签 */
    OLED_ShowString(10, 16, "HR:", OLED_8X16);
    OLED_ShowString(80, 16, "bpm", OLED_8X16);
    
#ifdef USE_MAX30102
    /* 血氧标签（仅MAX30102模式） */
    OLED_ShowString(10, 36, "SpO2:", OLED_8X16);
    OLED_ShowString(100, 36, "%", OLED_8X16);
#elif defined(USE_DS18B20)
    /* 温度标签（DS18B20模式，占用SpO2行空位） */
    OLED_ShowString(10, 36, "Temp:", OLED_8X16);
    OLED_ShowString(100, 36, "C", OLED_8X16);
#elif defined(USE_ECG_SIM)
    /* 模拟器BPM标签（仅USE_ECG_SIM模式，占用SpO2行空位） */
    OLED_ShowString(10, 36, "Sim:", OLED_8X16);
    OLED_ShowString(80, 36, "bpm", OLED_8X16);
#endif

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
#ifdef USE_MAX30102
    MAX30102_Data_t *data = MAX30102_GetData();
    uint16_t current_hr = data->heart_rate;
#else
    uint16_t current_hr = (uint16_t)ECG_GetHeartRate();
#endif

    /* 首次进入页面，绘制静态内容并全屏刷新 */
    if (!page0_static_drawn)
    {
        Display_Page0_DrawStatic();
        last_hr      = 0xFFFF;   /* 强制刷新数值 */
        last_sim_bpm = 0xFF;
#ifdef USE_MAX30102
        last_spo2   = 0xFFFF;
        last_finger = 0xFF;
#endif
#ifdef USE_DS18B20
        last_temp = -127.0f; /* 强制刷新温度 */
    last_temp_valid = 0;
#endif
        OLED_ShowNum(50, 16, current_hr, 3, OLED_8X16);
#ifdef USE_MAX30102
        OLED_ShowNum(60, 36, data->spo2, 3, OLED_8X16);
        OLED_ShowString(100, 0, data->finger_detected ? "OK" : "--", OLED_6X8);
#elif defined(USE_DS18B20)
        if (g_ds18b20_data.is_valid)
        {
            OLED_ShowFloatNum(58, 36, g_ds18b20_data.temperature, 2, 1, OLED_8X16);
        }
        else
        {
            /* 5字符宽度：对齐 OLED_ShowFloatNum 的 "+" + "xx" + "." + "x" */
            OLED_ShowString(58, 36, " --.-", OLED_8X16);
        }
#elif defined(USE_ECG_SIM)
    OLED_ShowNum(58, 36, ECG_Sim_GetBPM(), 3, OLED_8X16);
#endif
        OLED_Update();
        last_hr = current_hr;
#ifdef USE_MAX30102
        last_spo2   = data->spo2;
        last_finger = data->finger_detected;
#elif defined(USE_DS18B20)
    last_temp_valid = g_ds18b20_data.is_valid;
    last_temp = g_ds18b20_data.is_valid ? g_ds18b20_data.temperature : -127.0f;
#elif defined(USE_ECG_SIM)
    last_sim_bpm = ECG_Sim_GetBPM();
#endif
        return;
    }

    /* 检测心率变化时局部刷新 */
    if (current_hr != last_hr)
    {
        last_hr = current_hr;
        OLED_ShowNum(50, 16, current_hr, 3, OLED_8X16);
        OLED_UpdateArea(50, 16, 24, 16);
    }

#ifdef USE_MAX30102
    /* 血氧值变化时局部刷新 */
    if (data->spo2 != last_spo2)
    {
        last_spo2 = data->spo2;
        OLED_ShowNum(60, 36, data->spo2, 3, OLED_8X16);
        OLED_UpdateArea(60, 36, 24, 16);
    }
    /* 手指检测状态变化时局部刷新 */
    if (data->finger_detected != last_finger)
    {
        last_finger = data->finger_detected;
        OLED_ShowString(100, 0, data->finger_detected ? "OK" : "--", OLED_6X8);
        OLED_UpdateArea(100, 0, 12, 8);
    }
#elif defined(USE_DS18B20)
    /* 温度值变化时局部刷新 */
    if (g_ds18b20_data.is_valid)
    {
        if (!last_temp_valid || (g_ds18b20_data.temperature != last_temp))
        {
            last_temp_valid = 1;
            last_temp = g_ds18b20_data.temperature;
            OLED_ShowFloatNum(58, 36, last_temp, 2, 1, OLED_8X16);
            /* OLED_ShowFloatNum 会显示符号位：总宽 = (1 + IntLength + 1 + FraLength) * 8 = 40 */
            OLED_UpdateArea(58, 36, 40, 16);
        }
    }
    else
    {
        if (last_temp_valid)
        {
            last_temp_valid = 0;
            last_temp = -127.0f;
            OLED_ShowString(58, 36, " --.-", OLED_8X16);
            OLED_UpdateArea(58, 36, 40, 16);
        }
    }
#elif defined(USE_ECG_SIM)
    /* 模拟器BPM变化时局部刷新 */
    {
        uint8_t cur_sim = ECG_Sim_GetBPM();
        if (cur_sim != last_sim_bpm)
        {
            last_sim_bpm = cur_sim;
            OLED_ShowNum(58, 36, cur_sim, 3, OLED_8X16);
            OLED_UpdateArea(58, 36, 24, 16);
        }
    }
#endif
}

/*============================================================================*/
/*                              页面1: 心电图显示                              */
/*============================================================================*/

/**
 * @brief  页面1: 心电图显示
 */
void Display_Page1_ECG(void)
{
    uint8_t ecg_hr;

    /* 标题 */
    OLED_ShowString(0, 0, "ECG Monitor", OLED_6X8);
    
    /* 坐标系绘制 */
    OLED_DrawLine(1, 54, 120, 54);     /* X轴 */
    OLED_DrawLine(1, 10, 1, 54);       /* Y轴 */
    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);    /* Y轴箭头 */
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);  /* X轴箭头 */
    
    /* 显示测试时间 */
    OLED_ShowNum(70, 0, test, 3, OLED_6X8);
    OLED_ShowString(88, 0, "s", OLED_6X8);
    
    /* 显示ECG检测心率 */
#ifdef USE_ECG_SIM
    OLED_ShowNum(100, 0, ECG_Sim_GetBPM(), 3, OLED_6X8);
#else
    ecg_hr = ECG_GetHeartRate();
    OLED_ShowNum(100, 0, ecg_hr, 3, OLED_6X8);
#endif
    
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
 *          │ [DBG] T:XXXs  HR: XXX  │  y=0
 *          │────────────────────────│  y=10
 *          │ Loop: XXXXX 10us       │  y=14
 *          │ Max:  XXXXX 10us       │  y=24
 *          │ ADC:  XXXX   HR: XXX   │  y=34
 *          │ MAC:XX:XX:XX:XX:XX:XX  │  y=44
 *          │ [<] Page 3/3     [>]   │  y=56
 *          └────────────────────────┘
 */
void Display_Page2_Debug(void)
{
    uint16_t adc_raw;
#ifdef USE_MAX30102
    MAX30102_Data_t *data = MAX30102_GetData();
#endif

    adc_raw = AD_GetValue();

    /* 标题行: [DBG] T:XXXs  HR:XXX */
    OLED_ShowString(0, 0, "[DBG]T:", OLED_6X8);
    OLED_ShowNum(42, 0, test, 4, OLED_6X8);
    OLED_ShowString(66, 0, "s", OLED_6X8);
    OLED_ShowString(78, 0, "HR:", OLED_6X8);
#ifdef USE_MAX30102
    OLED_ShowNum(102, 0, data->heart_rate, 3, OLED_6X8);
#else
    OLED_ShowNum(102, 0, ECG_GetHeartRate(), 3, OLED_6X8);
#endif

    OLED_DrawLine(0, 10, 127, 10);

    /* 循环时间 */
    OLED_ShowString(0, 14, "Loop:", OLED_6X8);
    OLED_ShowNum(36, 14, display_loop_time_ms, 5, OLED_6X8);
    OLED_ShowString(72, 14, "10us", OLED_6X8);

    /* 最大循环时间 */
    OLED_ShowString(0, 24, "Max:", OLED_6X8);
    OLED_ShowNum(30, 24, display_loop_time_max_ms, 5, OLED_6X8);
    OLED_ShowString(66, 24, "10us", OLED_6X8);

    /* ADC原始值 + 模拟BPM / SpO2 */
    OLED_ShowString(0, 34, "ADC:", OLED_6X8);
    OLED_ShowNum(30, 34, adc_raw, 4, OLED_6X8);
#ifdef USE_MAX30102
    OLED_ShowString(66, 34, "SpO2:", OLED_6X8);
    OLED_ShowNum(102, 34, data->spo2, 3, OLED_6X8);
#elif defined(USE_ECG_SIM)
    OLED_ShowString(60, 34, "Sim:", OLED_6X8);
    OLED_ShowNum(90, 34, ECG_Sim_GetBPM(), 3, OLED_6X8);
#endif

    /* MAC地址行: "MAC:XX:XX:XX:XX:XX:XX" (4+17=21字符, 126px, 刚好适配128px) */
    OLED_ShowString(0, 44, "MAC:", OLED_6X8);
    OLED_ShowString(24, 44, esp8266_mac, OLED_6X8);

    /* 页码指示 */
    OLED_ShowString(0, 56, "<K1", OLED_6X8);
    OLED_ShowString(45, 56, "3/3", OLED_6X8);
    OLED_ShowString(110, 56, "K3>", OLED_6X8);

    OLED_Update();
}
#endif

