#ifndef __USART2_H
#define __USART2_H 

#ifdef USE_STDPERIPH_DRIVER
//#include "stm32f10x.h"
//#include "sys.h"
#include "stdint.h"
#include "stdio.h"	  
//////////////////////////////////////////////////////////////////////////////////	   
//������ֻ��ѧϰʹ�ã�δ���������ɣ��������������κ���;
//ALIENTEK��ӢSTM32������V3
//����3��ʼ������
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//�޸�����:2015/3/14
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) �������������ӿƼ����޹�˾ 2014-2024
//All rights reserved
//********************************************************************************
//�޸�˵��
//��
////////////////////////////////////////////////////////////////////////////////// 	

#define USART2_MAX_RECV_LEN		600					//�����ջ����ֽ���
#define USART2_MAX_SEND_LEN		600					//����ͻ����ֽ���
#define USART2_RX_EN 			1					//0,������;1,����.

extern uint8_t  USART2_RX_BUF[USART2_MAX_RECV_LEN]; 		//���ջ���,���USART3_MAX_RECV_LEN�ֽ�
extern uint16_t USART2_RX_STA;   						//��������״̬

void usart2_init(uint32_t bound);
void u2_printf(char* fmt,...);
#endif
#endif	 
  
















