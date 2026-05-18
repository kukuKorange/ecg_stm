#ifndef __DS18B20_H
#define __DS18B20_H

#include "stm32f10x.h"
#include <stdint.h>

/*============================================================================*/
/*                              引脚定义                                       */
/*============================================================================*/

/* DS18B20 DQ引脚定义 (单总线) */
#define DS18B20_DQ_GPIO_PORT        GPIOB
#define DS18B20_DQ_GPIO_CLK         RCC_APB2Periph_GPIOB
#define DS18B20_DQ_GPIO_PIN         GPIO_Pin_6

/*============================================================================*/
/*                              电气配置（重要）                              */
/*============================================================================*/

/**
 * @brief  DQ输出模式选择
 * @note
 * - 推荐（默认）：开漏输出(GPIO_Mode_Out_OD) + 外部上拉(通常4.7k)
 * - 可选：推挽输出(GPIO_Mode_Out_PP)
 *   仅用于“没有外部上拉/上拉太弱”时尝试作为强上拉。
 *   使用推挽时驱动会在读取/应答窗口切回输入以避免对拉，但仍不建议长期这样用。
 */
/* #define DS18B20_USE_PUSH_PULL */

/*============================================================================*/
/*                              DS18B20命令定义                               */
/*============================================================================*/

#define DS18B20_SKIP_ROM            0xCC    /* 跳过读ROM */
#define DS18B20_READ_ROM            0x33    /* 读ROM */
#define DS18B20_CONVERT_T           0x44    /* 启动温度转换 */
#define DS18B20_READ_SCRATCHPAD     0xBE    /* 读暂存器 */
#define DS18B20_WRITE_SCRATCHPAD    0x4E    /* 写暂存器 */
#define DS18B20_COPY_SCRATCHPAD     0x48    /* 复制暂存器 */
#define DS18B20_RECALL_E2           0xB8    /* 调用E2 */
#define DS18B20_READ_POWER_SUPPLY   0xB4    /* 读电源供应模式 */

/*============================================================================*/
/*                              数据结构定义                                   */
/*============================================================================*/

/**
 * @brief  DS18B20温度数据结构体
 * @note   用于存储DS18B20测得的温度数据
 */
typedef struct {
    float temperature;          /**< 温度值 (°C) */
    uint8_t is_valid;          /**< 数据有效标志: 1=有效, 0=无效 */
} DS18B20_Data_t;

/**
 * @brief  全局温度数据（供其他模块使用）
 */
extern DS18B20_Data_t g_ds18b20_data;

/*============================================================================*/
/*                              函数声明                                       */
/*============================================================================*/

/**
 * @brief  DS18B20初始化
 * @param  无
 * @retval 初始化是否成功: 0=失败, 1=成功
 */
uint8_t DS18B20_Init(void);

/**
 * @brief  DS18B20复位（发送复位脉冲，检测是否存在）
 * @param  无
 * @retval 是否检测到设备: 0=未检测到, 1=检测到
 */
uint8_t DS18B20_Reset(void);

/**
 * @brief  DS18B20读一个字节
 * @param  无
 * @retval 读取的字节数据
 */
uint8_t DS18B20_ReadByte(void);

/**
 * @brief  DS18B20写一个字节
 * @param  byte: 要写入的字节数据
 * @retval 无
 */
void DS18B20_WriteByte(uint8_t byte);

/**
 * @brief  DS18B20读温度值（浮点格式，精确到0.0625°C）
 * @param  无
 * @retval 温度值 (°C)
 */
float DS18B20_ReadTemperature(void);

/**
 * @brief  DS18B20启动温度转换（非阻塞式）
 * @param  无
 * @retval 无
 */
void DS18B20_StartConversion(void);

/**
 * @brief  DS18B20检查转换是否完成
 * @param  无
 * @retval 转换状态: 0=未完成, 1=已完成
 */
uint8_t DS18B20_IsConversionDone(void);

/**
 * @brief  DS18B20读温度（执行完整的转换读取流程）
 * @param  无
 * @retval 无 (结果存储在全局变量g_ds18b20_data中)
 */
void DS18B20_Process(void);

#endif /* __DS18B20_H */
