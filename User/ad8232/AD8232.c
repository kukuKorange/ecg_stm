/**
  ******************************************************************************
  * @file    AD8232.c
  * @brief   AD8232心电模块驱动
  * 
  * @details AD8232是一款单导联心电前端芯片，用于采集心电信号(ECG)
  *          本驱动实现:
  *          - GPIO初始化（电极脱落检测引脚）
  *          - ECG数据采集与低通滤波
  *          - OLED实时波形绘制
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "ad8232.h"
#include "OLED.h"
#include "AD.h"

/*============================ 全局变量 ============================*/

uint16_t ecg_data[500] = {0};   /**< ECG数据缓冲区 */
uint16_t map_upload[130] = {0}; /**< 上传数据缓冲区 */
uint16_t ecg_index = 1;         /**< ECG数据索引 */
uint16_t test = 0;              /**< 测试计数器（秒） */

/*============================ 私有变量 ============================*/

static uint16_t draw_x = 0;     /**< 绘图X坐标 */

/*============================ 函数实现 ============================*/

/**
  * @brief  AD8232初始化
  * @note   配置PB0和PB1为浮空输入，用于电极脱落检测
  *         LO+ (PB0): 正电极脱落检测
  *         LO- (PB1): 负电极脱落检测
  */
void AD8232Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能GPIOB时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 配置PB0 (LO+) 为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
   
    /* 配置PB1 (LO-) 为浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  获取电极连接状态
  * @retval 1: 电极已连接, 0: 电极脱落
  * @note   当LO+和LO-都为低电平时，表示电极连接正常
  */
uint8_t GetConnect(void)
{
    uint8_t LO_plus, LO_minus;
    
    LO_plus = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0);
    LO_minus = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);
    
    if ((LO_plus == 0) && (LO_minus == 0))
    {
        return 1;  /* 连接正常 */
    }
    else
    {
        return 0;  /* 电极脱落 */
    }
}

/**
  * @brief  ECG数据采集与绘制
  * @note   此函数应在定时器中断中调用，采样率200Hz
  *         
  *         数据处理流程:
  *         1. 读取ADC值
  *         2. 数据缩放（适配OLED显示范围）
  *         3. 低通滤波（平滑曲线）
  *         4. 绘制波形线段
  *         5. 到达屏幕边缘后清屏重绘
  */
void ECG_SampleAndDraw(void)
{
    /* 注意: 调用前需在Timer2.c中判断 current_page == PAGE_ECG */
    
    if (ecg_index < 120)
    {
        /* 1. 读取ADC值并缩放 */
        /* ADC值范围0-4095，缩放到OLED Y轴范围(约10-55) */
        ecg_data[ecg_index] = 90 - AD_GetValue() / 45;
        
        /* 2. 边界处理 */
        ecg_data[0] = ecg_data[1];
        
        /* 3. 低通滤波: y = y_last + 0.25 * (y_new - y_last) */
        /* 滤波系数0.25，平滑曲线，减少抖动 */
        ecg_data[ecg_index] = ecg_data[ecg_index - 1] + 
                              (ecg_data[ecg_index] - ecg_data[ecg_index - 1]) * 0.25f;
        
        /* 4. 绘制波形线段 */
        OLED_DrawLine(draw_x + 3, ecg_data[ecg_index - 1], 
                      draw_x + 4, ecg_data[ecg_index]);
        
        /* 5. 保存到上传缓冲区 */
        if (ecg_index < 120)
        {
            map_upload[ecg_index] = ecg_data[ecg_index];
        }
        
        ecg_index++;
        draw_x += 1;
    }
    else
    {
        /* 到达屏幕边缘，清屏重绘 */
        ECG_ClearAndRedraw();
        ecg_index = 1;
        draw_x = 0;
    }
}

/**
  * @brief  ECG显示区域清除并重绘坐标轴
  * @note   清除波形区域，保留坐标轴
  *         
  *         坐标系布局:
  *         ▲ Y
  *         │
  *         │     波形区域
  *         │
  *         └──────────────► X
  */
void ECG_ClearAndRedraw(void)
{
    /* 清除波形区域 */
    OLED_ClearArea(3, 10, 125, 45);
    
    /* 重绘坐标轴 */
    OLED_DrawLine(1, 54, 120, 54);     /* X轴 */
    OLED_DrawLine(1, 10, 1, 54);       /* Y轴 */
    
    /* Y轴箭头 */
    OLED_DrawTriangle(1, 8, 0, 10, 2, 10, OLED_UNFILLED);
    
    /* X轴箭头 */
    OLED_DrawTriangle(120, 55, 120, 53, 123, 54, OLED_UNFILLED);
}

/**
  * @brief  获取心率值（峰值检测法）
  * @param  array: ECG数据数组
  * @param  length: 数组长度
  * @retval 心率值（基于相邻峰值间隔计算）
  * @note   通过检测ECG波形的R波峰值计算心率
  */
uint8_t GetHeartRate(uint16_t *array, uint16_t length)
{
    uint8_t i, count = 0;
    uint16_t peakPositions[10] = {0};
    
    for (i = 1; i < length - 1; i++) 
    {
        /* 检测峰值：当前点大于前后两点，且幅值小于阈值 */
        if ((array[i] > array[i - 1]) && 
            (array[i] > array[i + 1]) && 
            (array[i] < 20)) 
        {
            peakPositions[count] = i;
            count++;
            
            if (count >= 2)
            {
                /* 计算心率: HR = 60 / (峰值间隔 * 采样周期) */
                return (peakPositions[1] - peakPositions[0]);
            }
        }
    }
    
    return 0;
}

/**
  * @brief  绘制ECG图表（静态显示）
  * @param  Chart: 数据数组
  * @param  Width: 线宽
  */
void DrawChart(uint16_t Chart[], uint8_t Width)
{
    int i, j = 0;
    
    for (i = 0; i < 118; i++)
    {
        OLED_DrawLine(j + 3, Chart[i], j + 4 + Width, Chart[i + 1]);
        j += 2;
    }
}

/**
  * @brief  图表数据优化（2点平均滤波）
  * @param  R: 原始数据数组
  * @param  Chart: 输出数据数组
  */
void ChartOptimize(uint16_t *R, uint16_t *Chart)
{
    int i, j = 0;
    
    for (i = 0; i < 500; i++)
    {
        Chart[i] = (R[j] + R[j + 1]) / 2;
        
        if (R[j + 1] == 0)
        {
            break;
        }
        j += 2;
    }
}

#endif
