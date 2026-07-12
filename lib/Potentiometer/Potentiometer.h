#ifndef POTENTIOMETER_H
#define POTENTIOMETER_H

#include <driver/adc.h>

// 电位器配置（默认 GPIO 1，可修改）
#define POT_ADC_CHANNEL ADC1_CHANNEL_0
#define POT_GPIO_PIN 1

void Pot_Init(void);

// 获取原始 ADC 值 (0-4095)
int Pot_GetRaw(void);

// 获取归一化值 (0.0 - 1.0)
float Pot_GetNormalized(void);

#endif
