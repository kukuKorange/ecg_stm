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
#ifdef USE_ECG_SIM
#include "ecg_sim.h"
#endif

/*============================ 全局变量 ============================*/

uint16_t ecg_data[500] = {0};   /**< ECG数据缓冲区（显示用，已缩放） */
uint16_t map_upload[130] = {0}; /**< 上传数据缓冲区（旧版兼容） */
uint16_t ecg_index = 1;         /**< ECG数据索引 */
uint16_t test = 0;              /**< 测试计数器（秒） */

/*============================ ECG上传缓存（双缓冲） ============================*/

#define ECG_UPLOAD_BUFFER_SIZE  600   /**< 上传缓存大小（3秒 @ 200Hz） */
#define ECG_UPLOAD_BATCH_SIZE   1     /**< 每批上传点数 */

/**
 * 双缓冲机制:
 * - fill_buffer:   正在采集的缓冲区（不断写入新数据）
 * - upload_buffer: 完整的600点数据，用于上传
 * - 采集满600点时自动交换，确保upload_buffer始终是完整数据
 */
static uint16_t ecg_buffer_a[ECG_UPLOAD_BUFFER_SIZE];  /**< 缓冲区A */
static uint16_t ecg_buffer_b[ECG_UPLOAD_BUFFER_SIZE];  /**< 缓冲区B */

static uint16_t *ecg_fill_buffer = ecg_buffer_a;       /**< 当前采集缓冲区 */
static uint16_t *ecg_upload_buffer = ecg_buffer_b;     /**< 当前上传缓冲区 */

static uint16_t ecg_fill_idx = 0;         /**< 采集缓冲区写入索引 */
static uint8_t  ecg_buffer_ready = 0;     /**< 上传缓冲区数据就绪标志 */

uint16_t ecg_upload_read_idx = 0;         /**< 上传读取索引 */
uint8_t  ecg_upload_active = 0;           /**< 上传进行中标志 */
uint32_t ecg_upload_timestamp = 0;        /**< 上传数据起始时间戳 */

/*============================ 私有变量 ============================*/

static uint16_t draw_x = 0;           /**< 绘图X坐标 */
static float last_filtered = 2048;    /**< 上一次滤波值（用于上传数据滤波）*/

/*============================ 心率检测变量 ============================*/

#define ECG_SAMPLE_RATE     200       /**< 采样率 (Hz) */
#define ECG_HR_MIN          30        /**< 最小有效心率 */
#define ECG_HR_MAX          220       /**< 最大有效心率 */
#define ECG_PEAK_THRESHOLD  2300      /**< R波峰值检测阈值（根据实际信号调整） */
#define ECG_REFRACTORY_MS   200       /**< 不应期 (ms)，防止重复检测 */
#define ECG_HR_FILTER_SIZE  4         /**< 心率滑动平均滤波窗口大小 */

static float ecg_prev_value = 2048;   /**< 上一个采样值（用于峰值检测） */
static float ecg_prev_prev_value = 2048; /**< 上上个采样值 */
static uint32_t ecg_sample_count = 0; /**< 采样计数器 */
static uint32_t ecg_last_peak_sample = 0; /**< 上一次R波的采样点 */
static uint8_t ecg_heart_rate = 0;    /**< 滤波后的心率 (bpm) */
static uint8_t ecg_peak_detected = 0; /**< 峰值检测标志（用于调试） */

/* 心率滑动平均滤波 */
static uint8_t ecg_hr_buffer[ECG_HR_FILTER_SIZE] = {0};  /**< 心率历史缓冲区 */
static uint8_t ecg_hr_buffer_idx = 0;    /**< 缓冲区写入索引 */
static uint8_t ecg_hr_buffer_count = 0;  /**< 缓冲区有效数据个数 */

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
  * @brief  获取ECG计算的心率
  * @retval 心率值 (bpm)，0表示尚未检测到有效心率
  */
uint8_t ECG_GetHeartRate(void)
{
    return ecg_heart_rate;
}

/**
  * @brief  ECG数据采集与绘制
  * @note   此函数应在定时器中断中调用，采样率200Hz
  *         
  *         数据处理流程:
  *         1. 读取ADC值
  *         2. 低通滤波
  *         3. 保存滤波后数据到上传缓存
  *         4. 数据缩放并绘制波形
  */
void ECG_SampleAndDraw(void)
{
    uint16_t adc_raw;
    float filtered;
    
    /* 注意: 调用前需在Timer2.c中判断 current_page == PAGE_ECG */
    
    /* 1. 获取ECG采样值（模拟或真实硬件） */
#ifdef USE_ECG_SIM
    adc_raw = ECG_Sim_GetSample();
#else
    adc_raw = AD_GetValue();
#endif
    
    /* 2. 低通滤波: y = y_last + 0.25 * (y_new - y_last) */
    filtered = last_filtered + ((float)adc_raw - last_filtered) * 0.4f;
    last_filtered = filtered;
    
    /* 3. 保存滤波后数据到采集缓冲区（双缓冲机制） */
    ecg_fill_buffer[ecg_fill_idx] = (uint16_t)filtered;
    ecg_fill_idx++;
    ecg_sample_count++;
    
    /* 4. R波峰值检测与心率计算 */
    {
        uint32_t refractory_samples = (ECG_REFRACTORY_MS * ECG_SAMPLE_RATE) / 1000;
        
        /* 峰值检测：当前点比前后两点都大，且超过阈值 */
        /* 使用上上个点作为"当前检测点"，因为需要看后一个点 */
        if ((ecg_sample_count > refractory_samples + ecg_last_peak_sample) &&  /* 不应期后 */
            (ecg_prev_value > ecg_prev_prev_value) &&    /* 前一点 < 当前点 */
            (ecg_prev_value > filtered) &&               /* 当前点 > 后一点（正在下降） */
            (ecg_prev_value > ECG_PEAK_THRESHOLD))       /* 超过阈值 */
        {
            /* 检测到 R 波峰值 */
            ecg_peak_detected = 1;
            
            if (ecg_last_peak_sample > 0)
            {
                /* 计算 RR 间期（采样点数） */
                uint32_t rr_samples = ecg_sample_count - ecg_last_peak_sample - 1;
                
                /* 换算心率: HR = 60 * 采样率 / RR间期 */
                uint16_t hr = (60 * ECG_SAMPLE_RATE) / rr_samples;
                
                /* 有效性检查后进行滑动平均滤波 */
                if (hr >= ECG_HR_MIN && hr <= ECG_HR_MAX)
                {
                    uint16_t sum = 0;
                    uint8_t i;
                    
                    /* 存入滤波缓冲区 */
                    ecg_hr_buffer[ecg_hr_buffer_idx] = (uint8_t)hr;
                    ecg_hr_buffer_idx = (ecg_hr_buffer_idx + 1) % ECG_HR_FILTER_SIZE;
                    if (ecg_hr_buffer_count < ECG_HR_FILTER_SIZE)
                    {
                        ecg_hr_buffer_count++;
                    }
                    
                    /* 计算平均值 */
                    for (i = 0; i < ecg_hr_buffer_count; i++)
                    {
                        sum += ecg_hr_buffer[i];
                    }
                    ecg_heart_rate = (uint8_t)(sum / ecg_hr_buffer_count);
                }
            }
            
            ecg_last_peak_sample = ecg_sample_count - 1;
        }
        else
        {
            ecg_peak_detected = 0;
        }
        
        /* 更新历史值 */
        ecg_prev_prev_value = ecg_prev_value;
        ecg_prev_value = filtered;
    }
    
    /* 5. 采集满600点，交换缓冲区 */
    if (ecg_fill_idx >= ECG_UPLOAD_BUFFER_SIZE)
    {
        uint16_t *temp;
        
        ecg_fill_idx = 0;
        
        /* 只有在上传完成后才交换缓冲区 */
        if (!ecg_upload_active)
        {
            temp = ecg_fill_buffer;
            ecg_fill_buffer = ecg_upload_buffer;
            ecg_upload_buffer = temp;
            ecg_buffer_ready = 1;  /* 标记有完整数据可上传 */
        }
    }
    
    /* 6. 绘制波形 */
    if (ecg_index < 120)
    {
        int16_t y_pos;
        
        /* 数据缩放（保持原比例） */
        y_pos = 90 - (int16_t)(filtered / 45);
        
        /* 边界限制：确保在坐标轴内 (Y: 11-53) */
        if (y_pos < 11) y_pos = 11;
        if (y_pos > 53) y_pos = 53;
        
        ecg_data[ecg_index] = (uint16_t)y_pos;
        ecg_data[0] = ecg_data[1];
        
        /* 绘制波形线段 */
        OLED_DrawLine(draw_x + 3, ecg_data[ecg_index - 1], 
                      draw_x + 4, ecg_data[ecg_index]);
        
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

/*============================================================================*/
/*                              ECG上传功能                                    */
/*============================================================================*/

/**
  * @brief  开始ECG数据上传
  * @param  timestamp: 数据起始时间戳
  * @note   调用后，ECG_UploadProcess() 会分批上传完整的600点数据
  * @retval 1: 开始上传, 0: 无数据可上传
  */
uint8_t ECG_StartUpload(uint32_t timestamp)
{
    /* 检查是否有完整的数据可上传 */
    if (!ecg_buffer_ready)
    {
        return 0;  /* 还没有完整的600点数据 */
    }
    
    ecg_upload_timestamp = timestamp;
    ecg_upload_read_idx = 0;
    ecg_upload_active = 1;
    ecg_buffer_ready = 0;  /* 清除就绪标志，等待下一次采集完成 */
    
    return 1;
}

/**
  * @brief  停止ECG数据上传
  */
void ECG_StopUpload(void)
{
    ecg_upload_active = 0;
    ecg_upload_read_idx = 0;
}

/**
  * @brief  获取待上传的数据量
  * @retval 缓存中的数据点数（始终为600或0）
  */
uint16_t ECG_GetUploadDataCount(void)
{
    if (ecg_upload_active || ecg_buffer_ready)
    {
        return ECG_UPLOAD_BUFFER_SIZE;
    }
    return 0;
}

/**
  * @brief  检查是否有完整数据可上传
  * @retval 1: 有完整600点数据, 0: 无
  */
uint8_t ECG_IsDataReady(void)
{
    return ecg_buffer_ready;
}

/**
  * @brief  读取上传缓冲区指定索引的数据
  * @param  index: 数据索引 (0-599)
  * @retval 该索引处的ECG值
  */
uint16_t ECG_GetUploadData(uint16_t index)
{
    if (index < ECG_UPLOAD_BUFFER_SIZE)
    {
        return ecg_upload_buffer[index];
    }
    return 0;
}

/**
  * @brief  获取上传缓冲区指针
  * @retval 上传缓冲区起始地址
  */
uint16_t* ECG_GetUploadBuffer(void)
{
    return ecg_upload_buffer;
}

/**
  * @brief  获取一批ECG数据用于上传
  * @param  batch_data: 输出缓冲区
  * @param  batch_size: 请求的批次大小
  * @retval 实际获取的数据点数（0表示上传完成）
  */
uint16_t ECG_GetUploadBatch(uint16_t *batch_data, uint16_t batch_size)
{
    uint16_t i;
    uint16_t count = 0;
    uint16_t available;
    
    if (!ecg_upload_active)
    {
        return 0;
    }
    
    /* 计算剩余数据量（固定600点） */
    available = ECG_UPLOAD_BUFFER_SIZE - ecg_upload_read_idx;
    if (available == 0)
    {
        /* 上传完成 */
        ecg_upload_active = 0;
        return 0;
    }
    
    /* 限制批次大小 */
    if (batch_size > available)
    {
        batch_size = available;
    }
    if (batch_size > ECG_UPLOAD_BATCH_SIZE)
    {
        batch_size = ECG_UPLOAD_BATCH_SIZE;
    }
    
    /* 从上传缓冲区复制数据 */
    for (i = 0; i < batch_size; i++)
    {
        batch_data[i] = ecg_upload_buffer[ecg_upload_read_idx + i];
        count++;
    }
    
    ecg_upload_read_idx += count;
    
    return count;
}

/**
  * @brief  获取上传进度
  * @retval 进度百分比 (0-100)
  */
uint8_t ECG_GetUploadProgress(void)
{
    if (!ecg_upload_active)
    {
        return 100;
    }
    return (uint8_t)((ecg_upload_read_idx * 100) / ECG_UPLOAD_BUFFER_SIZE);
}

/**
  * @brief  检查上传是否完成
  * @retval 1: 上传完成或未开始, 0: 上传进行中
  */
uint8_t ECG_IsUploadComplete(void)
{
    return !ecg_upload_active;
}

#endif
