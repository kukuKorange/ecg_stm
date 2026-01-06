#include "max30102.h"
#include "max30102_fir.h"
#include "./i2c/bsp_i2c.h"
#include "stm32f10x_exti.h"
#include "misc.h"

/*============================================================================*/
/*                              全局变量                                       */
/*============================================================================*/

/** @brief 心率血氧数据结构体（全局，供其他模块使用） */
MAX30102_Data_t g_max30102_data = {0, 0, 0, 0};

/*============================================================================*/
/*                              私有变量                                       */
/*============================================================================*/

static float ppg_data_cache_IR[HR_CACHE_NUMS] = {0};   /**< IR通道缓存 */
static float ppg_data_cache_RED[HR_CACHE_NUMS] = {0};  /**< RED通道缓存 */
static uint16_t cache_counter = 0;                     /**< 缓存计数器 */
static uint16_t hr_last = 0;                           /**< 上一次心率值（用于滤波）*/

/*============================================================================*/
/*                              私有函数                                       */
/*============================================================================*/

/**
 * @brief  低通滤波函数
 */
static float Lowpass(float X_last, float X_new, float K)
{
    return X_last + K * (X_new - X_last);
}

/**
  * @brief  MAX30102 I2C写寄存器
  * @param  reg_adder: 寄存器地址
  * @param  data: 写入数据
  */
void max30102_i2c_write(uint8_t reg_adder, uint8_t data)
{
	uint8_t transmit_data[2];
	transmit_data[0] = reg_adder;
	transmit_data[1] = data;
    i2c_transmit(transmit_data, 2);
}

/**
  * @brief  MAX30102 I2C读寄存器
  * @param  reg_adder: 寄存器地址
  * @param  pdata: 数据缓冲区
  * @param  data_size: 读取长度
  */
void max30102_i2c_read(uint8_t reg_adder, uint8_t *pdata, uint8_t data_size)
{
    uint8_t adder = reg_adder;
    i2c_transmit(&adder, 1);
    i2c_receive(pdata, data_size);
}

/**
  * @brief  MAX30102中断引脚初始化 (使用标准库)
  */
void max30102_int_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 使能GPIO时钟和AFIO时钟 */
    RCC_APB2PeriphClockCmd(MAX30102_INT_GPIO_CLK | RCC_APB2Periph_AFIO, ENABLE);

    /* 配置GPIO为上拉输入 */
    GPIO_InitStructure.GPIO_Pin = MAX30102_INT_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  /* 上拉输入 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MAX30102_INT_GPIO_Port, &GPIO_InitStructure);

    /* 配置EXTI */
    GPIO_EXTILineConfig(MAX30102_INT_EXTI_PortSource, MAX30102_INT_EXTI_PinSource);

    EXTI_InitStructure.EXTI_Line = MAX30102_INT_EXTI_Line;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  /* 下降沿触发 */
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* 配置NVIC */
    NVIC_InitStructure.NVIC_IRQChannel = MAX30102_INT_EXTI_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  MAX30102初始化
  */
void max30102_init(void)
{ 
	uint8_t data;
	
    I2cMaster_Init();   /* 初始化I2C接口 */
	delay_ms(500);
    
    max30102_int_gpio_init();   /* 中断引脚配置 */
    
    max30102_i2c_write(MODE_CONFIGURATION, 0x40);  /* reset the device */
	
	delay_ms(5);
	
    max30102_i2c_write(INTERRUPT_ENABLE1, 0xE0);
    max30102_i2c_write(INTERRUPT_ENABLE2, 0x00);  /* interrupt enable: FIFO almost full flag, new FIFO Data Ready,
                                                     ambient light cancellation overflow, power ready flag, 
                                                     internal temperature ready flag */
	
    max30102_i2c_write(FIFO_WR_POINTER, 0x00);
    max30102_i2c_write(FIFO_OV_COUNTER, 0x00);
    max30102_i2c_write(FIFO_RD_POINTER, 0x00);   /* clear the pointer */
	
    max30102_i2c_write(FIFO_CONFIGURATION, 0x4F); /* FIFO configuration: sample averaging(4), FIFO rolls on full(0), 
                                                     FIFO almost full value(15 empty data samples when interrupt is issued) */
	
    max30102_i2c_write(MODE_CONFIGURATION, 0x03);  /* MODE configuration: SpO2 mode */
	
    max30102_i2c_write(SPO2_CONFIGURATION, 0x2A); /* SpO2 configuration: ACD resolution:15.63pA, sample rate control:200Hz, 
                                                     LED pulse width:215 us */
	
    max30102_i2c_write(LED1_PULSE_AMPLITUDE, 0x2f);   /* IR LED */
    max30102_i2c_write(LED2_PULSE_AMPLITUDE, 0x2f);   /* RED LED current */
	
    max30102_i2c_write(TEMPERATURE_CONFIG, 0x01);     /* temp */
	
    max30102_i2c_read(INTERRUPT_STATUS1, &data, 1);
    max30102_i2c_read(INTERRUPT_STATUS2, &data, 1);  /* clear status */
}

/**
  * @brief  读取MAX30102 FIFO数据
  * @param  output_data: 输出数据 (float数组, 至少2个元素)
  */
void max30102_fifo_read(float *output_data)
{
    uint8_t receive_data[6];
	uint32_t data[2];
    max30102_i2c_read(FIFO_DATA, receive_data, 6);
    data[0] = ((receive_data[0]<<16 | receive_data[1]<<8 | receive_data[2]) & 0x03ffff);
    data[1] = ((receive_data[3]<<16 | receive_data[4]<<8 | receive_data[5]) & 0x03ffff);
	*output_data = data[0];
	*(output_data+1) = data[1];
}

/**
  * @brief  计算心率
  * @param  input_data: 输入PPG数据
  * @param  cache_nums: 数据点数量
  * @retval 心率值 (次/分)
  */
uint16_t max30102_getHeartRate(float *input_data, uint16_t cache_nums)
{
		float input_data_sum_aver = 0;
    uint16_t i, temp;
		
    for(i = 0; i < cache_nums; i++) {
        input_data_sum_aver += *(input_data + i);
		}
    input_data_sum_aver = input_data_sum_aver / cache_nums;
    
    for(i = 0; i < cache_nums; i++) {
        if((*(input_data + i) > input_data_sum_aver) && (*(input_data + i + 1) < input_data_sum_aver)) {
					temp = i;
					break;
				}
		}
		i++;
    for(; i < cache_nums; i++) {
        if((*(input_data + i) > input_data_sum_aver) && (*(input_data + i + 1) < input_data_sum_aver)) {
					temp = i - temp;
					break;
				}
		}
    
    if((temp > 14) && (temp < 100)) {
        /* SpO2采样频率200Hz，FIFO采样平均设置值为4，所以实际采集采样频率约为50Hz */
        return 3000 / temp;  /* 计算一次的间隔是0.02s，temp是一个心跳周期中的采样数，所以一次心跳是temp/50秒,一分钟心跳数是60/(temp/50) */
    } else {
			return 0;
		}
}

/**
  * @brief  计算血氧饱和度
  * @param  ir_input_data: IR通道数据
  * @param  red_input_data: RED通道数据
  * @param  cache_nums: 数据点数量
  * @retval 血氧值 (%)
  */
float max30102_getSpO2(float *ir_input_data, float *red_input_data, uint16_t cache_nums)
{
    float ir_max = *ir_input_data, ir_min = *ir_input_data;
    float red_max = *red_input_data, red_min = *red_input_data;
			float R;
			uint16_t i;
    
    for(i = 1; i < cache_nums; i++) {
        if(ir_max < *(ir_input_data + i)) {
            ir_max = *(ir_input_data + i);
				}
        if(ir_min > *(ir_input_data + i)) {
            ir_min = *(ir_input_data + i);
				}
        if(red_max < *(red_input_data + i)) {
            red_max = *(red_input_data + i);
				}
        if(red_min > *(red_input_data + i)) {
            red_min = *(red_input_data + i);
				}
			}
    
    R = ((ir_max - ir_min) * red_min) / ((red_max - red_min) * ir_min);
    return ((-45.060) * R * R + 30.354 * R + 94.845);
}

/*============================================================================*/
/*                              数据处理函数                                   */
/*============================================================================*/

/**
 * @brief  心率血氧数据处理
 * @note   在主循环中调用，完成数据采集、滤波和计算
 */
void MAX30102_Process(void)
{
    float max30102_data[2];
    float fir_output[2];
    
    /* 读取FIFO数据 */
    max30102_fifo_read(max30102_data);
    
    /* FIR滤波 */
    ir_max30102_fir(&max30102_data[0], &fir_output[0]);
    red_max30102_fir(&max30102_data[1], &fir_output[1]);
    
    /* 检测手指是否放置 */
    if ((max30102_data[0] > PPG_DATA_THRESHOLD) && (max30102_data[1] > PPG_DATA_THRESHOLD))
    {
        /* 手指检测到，缓存数据 */
        g_max30102_data.finger_detected = 1;
        
        ppg_data_cache_IR[cache_counter] = fir_output[0];
        ppg_data_cache_RED[cache_counter] = fir_output[1];
        cache_counter++;
        
        /* 缓存满，计算心率和血氧 */
        if (cache_counter >= HR_CACHE_NUMS)
        {
            cache_counter = 0;
            
            /* 计算心率（带低通滤波） */
            g_max30102_data.heart_rate =  (uint16_t)max30102_getHeartRate(ppg_data_cache_IR, HR_CACHE_NUMS);
            
            /* 计算血氧 */
            g_max30102_data.spo2 = (uint16_t)max30102_getSpO2(ppg_data_cache_IR, ppg_data_cache_RED, HR_CACHE_NUMS);
            
            /* 标记数据就绪 */
            g_max30102_data.data_ready = 1;
        }
    }
    else
    {
        /* 手指未检测到，重置状态 */
        cache_counter = 0;
        g_max30102_data.finger_detected = 0;
        g_max30102_data.heart_rate = 0;
        g_max30102_data.data_ready = 0;
    }
}

/**
 * @brief  获取心率血氧数据指针
 * @retval 指向 MAX30102_Data_t 结构体的指针
 */
MAX30102_Data_t* MAX30102_GetData(void)
{
    return &g_max30102_data;
}
