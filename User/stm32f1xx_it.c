/**
  ******************************************************************************
  * @file    stm32f1xx_it.c 
  * @brief   中断服务函数 - 使用STM32标准库
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "stm32f10x_exti.h"
#include "stm32f1xx_it.h" 

/* Private variables ---------------------------------------------------------*/
extern uint8_t max30102_int_flag;

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  */
void SysTick_Handler(void)
{
    /* 标准库不需要HAL_IncTick */
}

/**
  * @brief  This function handles EXTI9_5 interrupt.
  */
void EXTI9_5_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line5) != RESET)
    {
        max30102_int_flag = 1;
        EXTI_ClearITPendingBit(EXTI_Line5);
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
