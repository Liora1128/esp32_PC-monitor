#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <driver/gpio.h>

// 光敏传感器引脚配置
#define LIGHT_ADC_CHANNEL ADC_CHANNEL_0
#define LIGHT_GPIO_PIN     1

// 初始化光敏传感器
void LightSensor_Init(void);

// 获取原始 ADC 值 (0-4095)
int LightSensor_GetRaw(void);

// 获取归一化值 (0.0 - 1.0)
float LightSensor_GetNormalized(void);

#endif
