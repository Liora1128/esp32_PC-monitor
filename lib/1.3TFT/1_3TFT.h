#ifndef _1_3TFT_H_
#define _1_3TFT_H_

#include <LovyanGFX.hpp>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// 独立函数：初始化 LEDC 背光 PWM（在 app_main 里调用）
void TFT_BL_Init(void);

// 独立函数：设置背光亮度 (0.0 - 1.0)
void TFT_BL_SetBrightness(float brightness);

class LGFX_ESP32ST7789 : public lgfx::LGFX_Device {
public:
    LGFX_ESP32ST7789(void);
    void reset(void);
    void init(void);  // 调用父类初始化

private:
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
};

#endif
