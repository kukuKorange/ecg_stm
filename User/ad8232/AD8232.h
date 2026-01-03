#ifndef __AD8232_H
#define __AD8232_H

#include "stdint.h"

void DrawChart(uint16_t Chart[],uint8_t Width);
void ChartOptimize(uint16_t *R,uint16_t *Chart);
void AD8232Init(void);
uint8_t GetConnect(void);
uint8_t GetHeartRate(uint16_t *array, uint16_t length);
// float Lowpass(float X_last, float X_new, float K);

#endif

