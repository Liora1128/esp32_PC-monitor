#include "LightSensor.h"
#include <esp_adc/adc_oneshot.h>

static adc_oneshot_unit_handle_t adc_handle;

void LightSensor_Init(void) {
    // ADC 单次采样配置
    adc_oneshot_unit_init_cfg_t init_cfg = {};
    init_cfg.unit_id = ADC_UNIT_1;
    init_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    // 通道配置
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, LIGHT_ADC_CHANNEL, &chan_cfg);

}

int LightSensor_GetRaw(void) {
    int raw;
    adc_oneshot_read(adc_handle, LIGHT_ADC_CHANNEL, &raw);
    return raw;
}

float LightSensor_GetNormalized(void) {
    int raw = LightSensor_GetRaw();

    float normalized = 1.0f - ((float)raw / 4095.0f);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    return normalized;
}
