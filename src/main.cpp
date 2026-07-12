#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <LovyanGFX.hpp>
#include <driver/spi_master.h>

#include "1_3TFT.h"
#include "LightSensor.h"

static const char* TAG = "main";

LGFX_ESP32ST7789 tft;

// 彩虹渐变色条
void drawRainbowBar(int y, int h) {
    for (int i = 0; i < h; i++) {
        int color = tft.color888(i * 255 / h, 255 - i * 255 / h, 128 + i * 127 / h);
        tft.drawFastHLine(0, y + i, 240, color);
    }
}

// 演示画面
void demoScreen(void) {
    tft.fillScreen(TFT_BLACK);

    // 标题
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tft.println("Light Sensor Demo");

    // 彩虹条
    drawRainbowBar(20, 30);

    // 矩形
    tft.drawRect(10, 60, 100, 50, TFT_RED);
    tft.fillRect(120, 60, 100, 50, TFT_GREEN);
    tft.drawRoundRect(10, 120, 100, 50, 8, TFT_BLUE);
    tft.fillRoundRect(120, 120, 100, 50, 8, TFT_YELLOW);

    // 圆形
    tft.drawCircle(60, 200, 30, TFT_MAGENTA);
    tft.fillCircle(180, 200, 30, TFT_CYAN);

    // 文字
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(0, 240 - 16);
    tft.print("ESP32-S3 ST7789");
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "TFT + Light Sensor Demo starting...");

    // 初始化光敏传感器（ADC GPIO1）
    LightSensor_Init();

    // 初始化屏幕
    ESP_LOGI(TAG, "Init TFT...");
    tft.reset();
    ESP_LOGI(TAG, "Init Backlight...");
    TFT_BL_Init();
    ESP_LOGI(TAG, "Init LovyanGFX...");
    tft.init();

    // 显示初始画面
    demoScreen();

    // 设置初始亮度
    float initial_light = LightSensor_GetNormalized();
    TFT_BL_SetBrightness(initial_light);
    ESP_LOGI(TAG, "Initial light: %.2f, brightness: %.2f", initial_light, initial_light);

    ESP_LOGI(TAG, "Auto brightness control started...");

    // 主循环：读取光敏传感器，调节屏幕亮度
    while (1) {
        // 读取光敏传感器值 (0.0 - 1.0)
        float light = LightSensor_GetNormalized();
        light = light * 0.7f + 0.3f;

        // 设置屏幕亮度
        TFT_BL_SetBrightness(light);

        // ESP_LOGI(TAG, "Light: %.2f, Brightness: %.2f%%", light, light * 100);

        vTaskDelay(pdMS_TO_TICKS(200));  // 每200ms更新一次
    }
}
