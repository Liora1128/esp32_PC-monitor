#include "Potentiometer.h"

void Pot_Init(void) {
    // 配置 ADC 精度为 12 位 (0-4095)
    adc1_config_width(ADC_WIDTH_BIT_12);

    // 配置通道，衰减为 11dB，允许输入 0-3.3V
    adc1_config_channel_atten(POT_ADC_CHANNEL, ADC_ATTEN_DB_11);
}

int Pot_GetRaw(void) {
    return adc1_get_raw(POT_ADC_CHANNEL);  // 返回 0-4095
}

float Pot_GetNormalized(void) {
    int raw = adc1_get_raw(POT_ADC_CHANNEL);
    return (float)raw / 4095.0f;  // 返回 0.0 - 1.0
}
