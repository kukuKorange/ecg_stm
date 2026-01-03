#ifdef USE_STDPERIPH_DRIVER

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"     // ���� RCC ���趨��
#include "stm32f10x_gpio.h"
#include "ad8232.h"
#include "OLED.h"  

void AD8232Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	// GPIOBʱ��

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;	//��������
	GPIO_Init(GPIOB, &GPIO_InitStructure); //��ʼ��PB0
   
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//��������
	GPIO_Init(GPIOB, &GPIO_InitStructure);  //��ʼ��PB1
	
}
//�����ж�����״̬
uint8_t GetConnect(void)
{
	uint8_t B0,B1;
	B0=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_0);
	B1=GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1);
	if((B0==0)&&(B1==0))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

uint8_t GetHeartRate(uint16_t *array, uint16_t length)
{
	  uint8_t i, count = 0;
	  uint16_t *peakPositions;
    for (i = 1; i < length - 1; i++) 
	  {
			 if ((array[i] > array[i - 1]) && (array[i] > array[i + 1]) &&(array[i])<20) 
		 	 {
				 	 // Found a peak value
				 	 peakPositions[count] = i;
					 count++;
			 }
			 if(count==1) return (peakPositions[1]-peakPositions[0]);
	  }
		return 0;
}

void DrawChart(uint16_t Chart[],uint8_t Width)
{
	int i=0,j=0;
	for(i=0;i<118;i++)
	{
		
		OLED_DrawLine(j+3,Chart[i],j+4+Width,Chart[i+1]);
		j+=2;
	}
}

void ChartOptimize(uint16_t *R,uint16_t *Chart)
{
	int i=0,j=0;
	for(i=0;i<500;i++)
	{
		Chart[i]=(R[j]+R[j+1])/2;
		if(R[j+1]==0)
		{
			break;
		}
		j+=2;
	}
}


#endif
