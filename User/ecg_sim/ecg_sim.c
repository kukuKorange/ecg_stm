/**
  ******************************************************************************
  * @file    ecg_sim.c
  * @brief   ECG信号模拟器实现
  *
  * @details 算法说明:
  *   1. 以"千分位相位"(0-999)表示在一个心动周期内的位置，
  *      避免浮点运算，同时对不同BPM保持一致的波形比例。
  *
  *   2. 各段用分段函数生成:
  *      - P/T 波: 对称抛物线钟形 f(t) = 4*A*t*(T-t)/T^2
  *        (等效于 sin²(πt/T) 的整数近似，中心最大、两端为零)
  *      - QRS: 线性斜坡（上升/下降），贴近真实ECG的尖锐QRS复合波
  *      - 其余区段: 等电位基线 (0偏移)
  *
  *   3. 基线 ADC = 2048 (12位ADC中点)
  *      R波峰值 ADC = 2048 + 1500 = 3548, 高于 ECG_PEAK_THRESHOLD(2300)
  *      可被现有的R波检测算法正确识别并计算心率。
  ******************************************************************************
  */

#ifdef USE_STDPERIPH_DRIVER

#include "ecg_sim.h"

#ifdef USE_ECG_SIM

/*============================================================================*/
/*                              私有常量                                       */
/*============================================================================*/

#define ECG_SIM_SAMPLE_RATE  200    /**< 采样率(Hz)，须与 ECG_SAMPLE_RATE 一致 */
#define ECG_SIM_BASELINE     2048   /**< ADC基线值（12位ADC中点）               */

/* PQRST 各段的千分位边界（0-999，比例固定，与BPM无关） */
#define PM_P_START      80          /**< P波起始     */
#define PM_P_END        200         /**< P波结束      */
#define PM_Q_START      245         /**< Q波起始      */
#define PM_Q_END        275         /**< Q波结束      */
#define PM_R_RISE_START 275         /**< R波上升起始  */
#define PM_R_RISE_END   315         /**< R波峰值      */
#define PM_R_FALL_END   380         /**< R波下降结束  */
#define PM_S_END        460         /**< S波恢复结束  */
#define PM_T_START      510         /**< T波起始      */
#define PM_T_END        810         /**< T波结束      */

/* 各段幅度（相对于基线的ADC偏移量） */
#define AMP_P    250    /**< P波正向幅度   */
#define AMP_Q    200    /**< Q波负向幅度   */
#define AMP_R   1500    /**< R波正向幅度（基线+1500=3548，高于检测阈值2300） */
#define AMP_S    400    /**< S波负向幅度   */
#define AMP_T    500    /**< T波正向幅度   */

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static uint8_t  sim_bpm = ECG_SIM_BPM;    /**< 当前模拟心率              */
static uint16_t sim_cycle_len = 0;         /**< 单个心动周期的采样点数    */
static uint16_t sim_phase = 0;             /**< 当前采样在周期内的索引    */

/*============================================================================*/
/*                              私有函数                                       */
/*============================================================================*/

/**
 * @brief  计算指定千分位相位处的ECG电压偏移
 * @param  pm: 千分位相位 (0-999)
 * @retval 相对于基线的ADC偏移量（有符号，单位：ADC计数）
 *
 * @details 抛物线钟形公式推导:
 *   设 t 为段内相对位置 (0..T-1)，T 为段宽（千分位），A 为幅度
 *   f(t) = 4A * t*(T-t) / T²
 *   - t=0  : f=0  (起点)
 *   - t=T/2: f=A  (峰值，数学上等于 A·sin²(π·t/T) 的最大值)
 *   - t=T  : f=0  (终点)
 *   整数溢出校验: T_max=300, A_max=500 → 4*150*150*500 = 45,000,000 < INT32_MAX ✓
 */
static int16_t ecg_sim_offset(uint16_t pm)
{
    int32_t t, T, val;

    /* P 波: 圆滑正向隆起 */
    if (pm >= PM_P_START && pm < PM_P_END)
    {
        t = pm - PM_P_START;
        T = PM_P_END - PM_P_START;     /* 120 */
        val = (int32_t)4 * t * (T - t) * AMP_P / (T * T);
        return (int16_t)val;
    }

    /* Q 波: 小负向凹陷 */
    if (pm >= PM_Q_START && pm < PM_Q_END)
    {
        t = pm - PM_Q_START;
        T = PM_Q_END - PM_Q_START;     /* 30 */
        val = (int32_t)4 * t * (T - t) * AMP_Q / (T * T);
        return -(int16_t)val;
    }

    /* R 波上升: 线性急升 0 → +AMP_R */
    if (pm >= PM_R_RISE_START && pm < PM_R_RISE_END)
    {
        t = pm - PM_R_RISE_START;
        T = PM_R_RISE_END - PM_R_RISE_START;   /* 40 */
        return (int16_t)((int32_t)t * AMP_R / T);
    }

    /* R 波下降 + S 波: 线性急降 +AMP_R → -AMP_S */
    if (pm >= PM_R_RISE_END && pm < PM_R_FALL_END)
    {
        t = pm - PM_R_RISE_END;
        T = PM_R_FALL_END - PM_R_RISE_END;     /* 65 */
        /* 从 +AMP_R 线性降至 -AMP_S */
        return (int16_t)(AMP_R - (int32_t)t * (AMP_R + AMP_S) / T);
    }

    /* S 恢复: 线性回升 -AMP_S → 0 */
    if (pm >= PM_R_FALL_END && pm < PM_S_END)
    {
        t = pm - PM_R_FALL_END;
        T = PM_S_END - PM_R_FALL_END;          /* 80 */
        return (int16_t)(-AMP_S + (int32_t)t * AMP_S / T);
    }

    /* T 波: 宽圆滑正向隆起 */
    if (pm >= PM_T_START && pm < PM_T_END)
    {
        t = pm - PM_T_START;
        T = PM_T_END - PM_T_START;             /* 300 */
        val = (int32_t)4 * t * (T - t) * AMP_T / (T * T);
        return (int16_t)val;
    }

    /* PR段 / ST段 / TP段: 等电位基线 */
    return 0;
}

/*============================================================================*/
/*                              公开函数                                       */
/*============================================================================*/

/**
 * @brief  初始化ECG信号模拟器
 */
void ECG_Sim_Init(uint8_t bpm)
{
    if (bpm < 30)  bpm = 30;
    if (bpm > 200) bpm = 200;

    sim_bpm = bpm;
    /* 每个心动周期的采样点数 = 采样率(Hz) * 60(s/min) / 心率(bpm) */
    sim_cycle_len = (uint16_t)((uint32_t)ECG_SIM_SAMPLE_RATE * 60u / bpm);
    sim_phase = 0;
}

/**
 * @brief  获取下一个模拟ECG采样值
 */
uint16_t ECG_Sim_GetSample(void)
{
    int32_t adc;
    uint16_t pm;

    /* 计算当前采样点在周期内的千分位相位 */
    pm = (uint16_t)((uint32_t)sim_phase * 1000u / sim_cycle_len);

    /* 基线 + PQRST偏移量 */
    adc = (int32_t)ECG_SIM_BASELINE + ecg_sim_offset(pm);

    /* 限幅到12位ADC范围 */
    if (adc < 0)    adc = 0;
    if (adc > 4095) adc = 4095;

    /* 推进相位，到周期末尾时归零 */
    sim_phase++;
    if (sim_phase >= sim_cycle_len)
    {
        sim_phase = 0;
    }

    return (uint16_t)adc;
}

/**
 * @brief  设置模拟心率（运行时可调）
 */
void ECG_Sim_SetBPM(uint8_t bpm)
{
    ECG_Sim_Init(bpm);
}

/**
 * @brief  获取当前模拟心率
 */
uint8_t ECG_Sim_GetBPM(void)
{
    return sim_bpm;
}

#endif /* USE_ECG_SIM */
#endif /* USE_STDPERIPH_DRIVER */
