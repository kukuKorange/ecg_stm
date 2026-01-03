/**
  ******************************************************************************
  * @file    max30102_fir.c
  * @brief   MAX30102 FIR数字滤波器实现
  * 
  * @details FIR (Finite Impulse Response) 有限脉冲响应滤波器
  *          用于滤除MAX30102采集的PPG信号中的高频噪声
  *          
  *          滤波器参数:
  *          - 类型: 低通滤波器
  *          - 阶数: 29阶
  *          - 采样率: 50Hz (MAX30102 200Hz采样，4倍平均后为50Hz)
  *          - 截止频率: 约5Hz (保留心率信号0.5-4Hz，滤除高频噪声)
  *          
  *          滤波器系数通过MATLAB的fdatool工具生成
  ******************************************************************************
  */

#include "max30102_fir.h"

/*============================ 宏定义 ============================*/

#define BLOCK_SIZE    1     /**< 每次调用arm_fir_f32处理的采样点数 */
#define NUM_TAPS      29    /**< FIR滤波器阶数（抽头数/系数个数）*/

/*============================ 私有变量 ============================*/

uint32_t blockSize = BLOCK_SIZE;
uint32_t numBlocks = BLOCK_SIZE;    /**< 需要调用arm_fir_f32的次数 */

/**
 * @brief FIR滤波器实例
 * @note  IR通道和RED通道各需要一个独立的滤波器实例
 *        因为每个滤波器需要维护自己的状态缓冲区
 */
arm_fir_instance_f32 S_ir;      /**< IR通道滤波器实例 */
arm_fir_instance_f32 S_red;     /**< RED通道滤波器实例 */

/**
 * @brief FIR滤波器状态缓冲区
 * @note  状态缓冲区大小 = numTaps + blockSize - 1
 *        用于存储滤波器的历史输入数据
 */
static float firStateF32_ir[BLOCK_SIZE + NUM_TAPS - 1];     /**< IR通道状态缓冲区 */
static float firStateF32_red[BLOCK_SIZE + NUM_TAPS - 1];    /**< RED通道状态缓冲区 */

/**
 * @brief 低通滤波器系数（通过MATLAB fdatool生成）
 * 
 * @details 滤波器特性:
 *          - 窗函数: 汉明窗 (Hamming Window)
 *          - 对称结构: 线性相位FIR滤波器
 *          - 系数呈对称分布: h[n] = h[N-1-n]
 *          
 *          系数分布图示:
 *          
 *               ^
 *          0.12 │        ▲▲▲
 *               │      ▲▲   ▲▲
 *          0.06 │    ▲▲       ▲▲
 *               │  ▲▲           ▲▲
 *             0 ├──●───────────────●──►
 *               │▼▼                 ▼▼
 *               └─────────────────────
 *                0                  28 (阶数)
 */
const float firCoeffs32LP[NUM_TAPS] = {
    -0.001542701735, -0.002211477375, -0.003286228748, -0.00442651147,  -0.004758632276,
    -0.003007677384,  0.002192312852,  0.01188309677,   0.02637642808,   0.04498152807,
     0.06596207619,   0.0867607221,    0.1044560149,    0.1163498312,    0.1205424443,   /* 中心系数最大 */
     0.1163498312,    0.1044560149,    0.0867607221,    0.06596207619,   0.04498152807,
     0.02637642808,   0.01188309677,   0.002192312852, -0.003007677384, -0.004758632276,
    -0.00442651147,  -0.003286228748, -0.002211477375, -0.001542701735
};

/*============================ 函数实现 ============================*/

/**
 * @brief  FIR滤波器初始化
 * 
 * @details 初始化IR和RED两个通道的FIR滤波器实例
 *          使用ARM CMSIS-DSP库的arm_fir_init_f32函数
 *          
 *          初始化后的滤波器结构:
 *          
 *          输入 ──► [Z⁻¹]──► [Z⁻¹]──► ... ──► [Z⁻¹]
 *                     │        │               │
 *                     ▼        ▼               ▼
 *                   [h₀]     [h₁]    ...    [h₂₈]
 *                     │        │               │
 *                     └────────┴───────────────┘
 *                               │
 *                               ▼ Σ
 *                             输出
 * 
 * @param  无
 * @retval 无
 */
void max30102_fir_init(void)
{
    /* 初始化IR通道FIR滤波器 */
    arm_fir_init_f32(&S_ir,                     /* 滤波器实例指针 */
                     NUM_TAPS,                   /* 滤波器阶数 */
                     (float32_t *)&firCoeffs32LP[0],  /* 滤波器系数数组 */
                     &firStateF32_ir[0],         /* 状态缓冲区 */
                     blockSize);                 /* 每次处理的采样点数 */
    
    /* 初始化RED通道FIR滤波器 */
    arm_fir_init_f32(&S_red,                    /* 滤波器实例指针 */
                     NUM_TAPS,                   /* 滤波器阶数 */
                     (float32_t *)&firCoeffs32LP[0],  /* 滤波器系数数组 */
                     &firStateF32_red[0],        /* 状态缓冲区 */
                     blockSize);                 /* 每次处理的采样点数 */
}

/**
 * @brief  IR通道FIR滤波
 * 
 * @details 对IR（红外）通道的PPG信号进行低通滤波
 *          滤除高频噪声，保留心率信号成分
 *          
 *          滤波公式:
 *          y[n] = Σ(h[k] * x[n-k]), k = 0 to NUM_TAPS-1
 *          
 *          其中:
 *          - x[n]: 输入信号（当前采样值）
 *          - y[n]: 输出信号（滤波后的值）
 *          - h[k]: 滤波器系数
 * 
 * @param  input:  输入数据指针（原始PPG数据）
 * @param  output: 输出数据指针（滤波后的数据）
 * @retval 无
 */
void ir_max30102_fir(float *input, float *output)
{
    arm_fir_f32(&S_ir,      /* 滤波器实例 */
                input,       /* 输入数据 */
                output,      /* 输出数据 */
                blockSize);  /* 处理的采样点数 */
}

/**
 * @brief  RED通道FIR滤波
 * 
 * @details 对RED（红光）通道的PPG信号进行低通滤波
 *          滤除高频噪声，保留心率信号成分
 *          
 *          RED通道主要用于血氧饱和度(SpO2)的计算:
 *          SpO2 = f(AC_red/DC_red, AC_ir/DC_ir)
 * 
 * @param  input:  输入数据指针（原始PPG数据）
 * @param  output: 输出数据指针（滤波后的数据）
 * @retval 无
 */
void red_max30102_fir(float *input, float *output)
{
    arm_fir_f32(&S_red,     /* 滤波器实例 */
                input,       /* 输入数据 */
                output,      /* 输出数据 */
                blockSize);  /* 处理的采样点数 */
}
