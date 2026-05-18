/**
 ******************************************************************************
 * @file    ds18b20.c
 * @author  Generated from reference code
 * @version V1.0
 * @date    2025-05-18
 * @brief   DS18B20数字温度计驱动程序（单总线通讯）
 *          - 支持读取温度（精确到0.0625°C）
 *          - 支持阻塞式和非阻塞式温度转换
 *          - 集成到STM32F103工程架构中
 ******************************************************************************
 */

#include "ds18b20.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/* TIM3 10us计数器：在 Timer3 中断里每10us +1 */
extern volatile uint32_t tim3_ms_counter;

/* 标准外设库：用于更新 SystemCoreClock，避免延时计算用到旧值 */
extern void SystemCoreClockUpdate(void);

/*============================================================================*/
/*                              全局变量                                       */
/*============================================================================*/

/** @brief 温度数据结构体（全局，供其他模块使用） */
DS18B20_Data_t g_ds18b20_data = {0.0f, 0};

/*============================================================================*/
/*                              私有宏定义                                     */
/*============================================================================*/

/** @brief DS18B20引脚读写（开漏，总线需要上拉） */
#define DS18B20_DQ_IN()     GPIO_ReadInputDataBit(DS18B20_DQ_GPIO_PORT, DS18B20_DQ_GPIO_PIN)
#define DS18B20_DQ_OUT_0()  GPIO_ResetBits(DS18B20_DQ_GPIO_PORT, DS18B20_DQ_GPIO_PIN)
#define DS18B20_DQ_OUT_1()  GPIO_SetBits(DS18B20_DQ_GPIO_PORT, DS18B20_DQ_GPIO_PIN)

/*============================================================================*/
/*                              DWT延时（us级）                                */
/*============================================================================*/

static void DS18B20_DWT_Init(void)
{
    /* 更新 SystemCoreClock（很多工程只配时钟但没调用 Update，会导致延时严重偏差） */
    SystemCoreClockUpdate();

    /* 使能DWT CYCCNT（Cortex-M3支持） */
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) == 0)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    }
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/*============================================================================*/
/*                              延时函数                                       */
/*============================================================================*/

/**
 * @brief  微秒级延时函数
 * @param  us: 延时微秒数
 * @retval 无
 * @note   基于72MHz系统时钟
 */
static void DS18B20_DelayUs(uint32_t us)
{
    /* DWT计数：避免用空循环导致时序漂移 */
    uint32_t start;
    uint32_t cycles;

    /* 若未开启CYCCNT，先初始化一次 */
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0)
    {
        DS18B20_DWT_Init();
    }

    cycles = us * (SystemCoreClock / 1000000U);
    start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
        /* wait */
    }
}

/**
 * @brief  毫秒级延时函数
 * @param  ms: 延时毫秒数
 * @retval 无
 */
static void DS18B20_DelayMs(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++)
        DS18B20_DelayUs(1000);
}

/*============================================================================*/
/*                              CRC8校验                                       */
/*============================================================================*/

static uint8_t DS18B20_CalcCrc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    uint8_t i;

    while (len--)
    {
        uint8_t inbyte = *data++;
        for (i = 0; i < 8; i++)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix)
                crc ^= 0x8C; /* 0x31多项式反转 */
            inbyte >>= 1;
        }
    }
    return crc;
}

static uint8_t DS18B20_TryReadTemperature(float *outTemp)
{
    uint8_t scratchpad[9];
    uint8_t i;
    uint8_t crc;
    int16_t raw;

    if (outTemp == 0)
        return 0;

    if (DS18B20_Reset() == 0)
        return 0;

    DS18B20_WriteByte(DS18B20_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_READ_SCRATCHPAD);

    for (i = 0; i < 9; i++)
    {
        scratchpad[i] = DS18B20_ReadByte();
    }

    crc = DS18B20_CalcCrc8(scratchpad, 8);
    if (crc != scratchpad[8])
        return 0;

    raw = (int16_t)((((uint16_t)scratchpad[1]) << 8) | scratchpad[0]);
    *outTemp = (float)raw / 16.0f;
    return 1;
}

/*============================================================================*/
/*                              单总线通讯函数                                 */
/*============================================================================*/

/**
 * @brief  DS18B20 GPIO配置为输出模式（开漏输出）
 * @param  无
 * @retval 无
 */
static void DS18B20_DQ_Output_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Pin = DS18B20_DQ_GPIO_PIN;
#ifdef DS18B20_USE_PUSH_PULL
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    /* 推挽输出（强上拉用，谨慎） */
#else
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;    /* 开漏输出（推荐） */
#endif
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_DQ_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief  DS18B20 GPIO配置为输入模式
 * @param  无
 * @retval 无
 */
static void DS18B20_DQ_Input_Mode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Pin = DS18B20_DQ_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;       /* 上拉输入 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_DQ_GPIO_PORT, &GPIO_InitStructure);
}

/*============================================================================*/
/*                              DS18B20核心驱动函数                            */
/*============================================================================*/

/**
 * @brief  DS18B20初始化
 * @param  无
 * @retval 初始化是否成功: 0=失败, 1=成功
 */
uint8_t DS18B20_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 初始化DWT计数器，用于us级延时 */
    DS18B20_DWT_Init();
    
    /* 使能GPIO时钟 */
    RCC_APB2PeriphClockCmd(DS18B20_DQ_GPIO_CLK, ENABLE);
    
    /* 配置DQ引脚为开漏输出模式 */
    GPIO_InitStructure.GPIO_Pin = DS18B20_DQ_GPIO_PIN;
#ifdef DS18B20_USE_PUSH_PULL
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    /* 推挽输出（强上拉用，谨慎） */
#else
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;    /* 开漏输出（推荐） */
#endif
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_DQ_GPIO_PORT, &GPIO_InitStructure);
    
    /* 释放DQ线（上拉） */
    DS18B20_DQ_OUT_1();
    DS18B20_DelayUs(2);
    
    /* 检测DS18B20是否存在 */
    return DS18B20_Reset();
}

/**
 * @brief  DS18B20复位（发送复位脉冲，检测是否存在）
 * @param  无
 * @retval 是否检测到设备: 0=未检测到, 1=检测到
 * @note   复位时序：
 *         1. 主机将DQ线拉低，保持至少480μs
 *         2. 主机释放DQ线，等待15~60μs
 *         3. 如果DS18B20存在，会将DQ线拉低60~240μs作为应答脉冲
 */
uint8_t DS18B20_Reset(void)
{
    uint16_t t;
    uint8_t present = 0;

    /* 主机将DQ线拉低 >= 480us */
    DS18B20_DQ_Output_Mode();
    DS18B20_DQ_OUT_0();
    DS18B20_DelayUs(500);

    /* 主机释放DQ线，切到输入（带上拉） */
    DS18B20_DQ_OUT_1();
    DS18B20_DQ_Input_Mode();

    /* 等待 15~60us 后，DS18B20 会在此窗口内拉低(60~240us)作为 presence */
    DS18B20_DelayUs(15);

    /* 在约 240us 窗口内轮询，捕获是否出现过低电平 */
    for (t = 0; t < 240; t++)
    {
        if (DS18B20_DQ_IN() == Bit_RESET)
        {
            present = 1;
            break;
        }
        DS18B20_DelayUs(1);
    }

    /* 等待 presence 脉冲结束（总线回到高电平），避免影响后续时序 */
    for (t = 0; t < 480; t++)
    {
        if (DS18B20_DQ_IN() == Bit_SET)
        {
            break;
        }
        DS18B20_DelayUs(1);
    }

    return present;
}

/**
 * @brief  DS18B20读一个位
 * @param  无
 * @retval 读取的位值: 0 或 1
 * @note   读时序：
 *         1. 主机将DQ线拉低，保持至少1μs
 *         2. 主机释放DQ线
 *         3. 在15μs内对DQ线采样
 */
static uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit = 0;

    DS18B20_DQ_Output_Mode();
    /* 主机拉低DQ线 */
    DS18B20_DQ_OUT_0();
    DS18B20_DelayUs(3);     /* 延时 >= 1us */

    /* 主机释放DQ线并切换为输入，在15us内采样 */
    DS18B20_DQ_OUT_1();
    DS18B20_DQ_Input_Mode();
    DS18B20_DelayUs(10);
    
    /* 采样DQ线状态 */
    bit = DS18B20_DQ_IN();
    
    /* 等待读周期结束（总时间至少60us） */
    DS18B20_DelayUs(50);
    
    return bit;
}

/**
 * @brief  DS18B20写一个位
 * @param  bit: 要写入的位值 (0 或 1)
 * @retval 无
 * @note   写时序：
 *         1. 主机将DQ线拉低，保持至少1μs
 *         2. 在15us内将数据送到总线上
 *         3. 如果写1，主机释放总线
 *         4. 如果写0，主机继续保持总线低电平
 */
static void DS18B20_WriteBit(uint8_t bit)
{
    DS18B20_DQ_Output_Mode();
    /* 主机拉低DQ线 */
    DS18B20_DQ_OUT_0();
    DS18B20_DelayUs(3);     /* 延时 >= 1us */
    
    if (bit)
    {
        /* 写1：推荐做法是释放总线由上拉拉高。
           若启用推挽，则在本时隙内保持驱动高电平作为“强上拉”。 */
        DS18B20_DQ_OUT_1();
    #ifndef DS18B20_USE_PUSH_PULL
        DS18B20_DQ_Input_Mode();
    #endif
    }
    /* 写0：主机继续保持DQ线低电平 */
    
    /* 完成一个写周期 */
    DS18B20_DelayUs(60);    /* 总延时 > 60us */
    DS18B20_DQ_OUT_1();     /* 释放总线 */
    DS18B20_DQ_Input_Mode();
    DS18B20_DelayUs(2);     /* 等待恢复时间 */
}

/**
 * @brief  DS18B20读一个字节
 * @param  无
 * @retval 读取的字节数据
 * @note   一个字节由8个位组成，LSB先传输
 */
uint8_t DS18B20_ReadByte(void)
{
    uint8_t i = 0;
    uint8_t byte = 0;
    
    for (i = 0; i < 8; i++)
    {
        /* LSB先传输：第i位对应先读到的位 */
        if (DS18B20_ReadBit())
            byte |= (1U << i);
    }
    
    return byte;
}

/**
 * @brief  DS18B20写一个字节
 * @param  byte: 要写入的字节数据
 * @retval 无
 * @note   一个字节由8个位组成，LSB先传输
 */
void DS18B20_WriteByte(uint8_t byte)
{
    uint8_t i = 0;
    
    for (i = 0; i < 8; i++)
    {
        /* LSB先传输 */
        DS18B20_WriteBit(byte & 0x01);
        byte >>= 1;
    }
}

/*============================================================================*/
/*                              DS18B20功能函数                                */
/*============================================================================*/

/**
 * @brief  DS18B20启动温度转换（非阻塞式）
 * @param  无
 * @retval 无
 * @note   转换时间约750ms
 */
void DS18B20_StartConversion(void)
{
    DS18B20_Reset();
    DS18B20_WriteByte(DS18B20_SKIP_ROM);          /* 跳过ROM，直接发送命令 */
    DS18B20_WriteByte(DS18B20_CONVERT_T);         /* 启动温度转换 */
}

/**
 * @brief  DS18B20检查转换是否完成
 * @param  无
 * @retval 转换状态: 0=未完成, 1=已完成
 * @note   通过读取暂存器的第0位来判断：1=转换完成，0=转换进行中
 */
uint8_t DS18B20_IsConversionDone(void)
{
    uint8_t status;
    
    DS18B20_Reset();
    DS18B20_WriteByte(DS18B20_SKIP_ROM);          /* 跳过ROM */
    DS18B20_WriteByte(DS18B20_READ_SCRATCHPAD);   /* 读暂存器 */
    
    /* 读取字节0-1（温度数据） */
    DS18B20_ReadByte();
    DS18B20_ReadByte();
    
    /* 读取字节4（配置寄存器），第0位为转换完成标志 */
    status = DS18B20_ReadByte();
    
    return (status & 0x01) ? 0 : 1;  /* 1=转换完成，0=转换中 */
}

/**
 * @brief  DS18B20读温度值（浮点格式，精确到0.0625°C）
 * @param  无
 * @retval 温度值 (°C)
 * @note   返回格式：高8位为整数部分，低4位为小数部分
 *         精度：0.0625°C per bit
 */
float DS18B20_ReadTemperature(void)
{
    float t = 0.0f;
    if (DS18B20_TryReadTemperature(&t))
        return t;
    return 0.0f;
}

/**
 * @brief  DS18B20读温度（执行完整的转换读取流程）
 * @param  无
 * @retval 无
 * @note   结果存储在全局变量g_ds18b20_data中
 *         这是一个完整的非阻塞式读取流程
 */
void DS18B20_Process(void)
{
    static uint8_t state = 0;      /* 状态机状态 */
    static uint32_t conversion_start = 0;  /* 转换开始时间（10us计数） */
    
    switch (state)
    {
        case 0:
            /* 启动温度转换 */
            if (DS18B20_Reset() == 0)
            {
                g_ds18b20_data.is_valid = 0;
                break;
            }
            DS18B20_WriteByte(DS18B20_SKIP_ROM);
            DS18B20_WriteByte(DS18B20_CONVERT_T);
            conversion_start = tim3_ms_counter;
            state = 1;
            break;
            
        case 1:
            /* 等待转换完成（约750ms，使用TIM3 10us计数器） */
            if ((uint32_t)(tim3_ms_counter - conversion_start) >= 75000U)
            {
                state = 2;
            }
            break;
            
        case 2:
            /* 读取温度值 */
            {
                float t;
                if (DS18B20_TryReadTemperature(&t))
                {
                    g_ds18b20_data.temperature = t;
                    g_ds18b20_data.is_valid = 1;
                }
                else
                {
                    g_ds18b20_data.is_valid = 0;
                }
            }
            state = 0;  /* 回到初始状态，准备下一次转换 */
            break;
            
        default:
            state = 0;
            break;
    }
}
