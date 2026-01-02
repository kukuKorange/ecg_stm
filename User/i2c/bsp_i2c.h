#ifndef _BSP_I2C_H
#define _BSP_I2C_H


#include <inttypes.h>
#include "main.h"
extern I2C_HandleTypeDef I2C_Handle;
#define  i2c_transmit(pdata,data_size)              HAL_I2C_Master_Transmit(&I2C_Handle,I2C_WRITE_ADDR,pdata,data_size,10)
#define  i2c_receive(pdata,data_size)   			HAL_I2C_Master_Receive(&I2C_Handle,I2C_READ_ADDR,pdata,data_size,10)
#define  delay_ms(ms)                               HAL_Delay(ms)

/* 定义I2C总线连接的GPIO端口, 用户只需要修改下面4行代码即可任意改变SCL和SDA的引脚 */

#define SENSORS_I2C              		      I2C1
#define SENSORS_I2C_RCC_CLK_ENABLE()   	 __HAL_RCC_I2C1_CLK_ENABLE()
#define SENSORS_I2C_FORCE_RESET()    		 __HAL_RCC_I2C1_FORCE_RESET()
#define SENSORS_I2C_RELEASE_RESET()  		 __HAL_RCC_I2C1_RELEASE_RESET()

/*引脚定义*/ 
#define SENSORS_I2C_SCL_GPIO_PORT         GPIOB
#define SENSORS_I2C_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define SENSORS_I2C_SCL_GPIO_PIN         	GPIO_PIN_6
 
#define SENSORS_I2C_SDA_GPIO_PORT         GPIOB
#define SENSORS_I2C_SDA_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOB_CLK_ENABLE()
#define SENSORS_I2C_SDA_GPIO_PIN          GPIO_PIN_7

#define SENSORS_I2C_AF                  	 GPIO_AF4_I2C2




void I2cMaster_Init(void);




#endif

