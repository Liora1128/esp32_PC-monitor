#include "1_3TFT.h"


void TFT_BL_Init(void)
{
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num  = LEDC_TIMER_0;
    timer.duty_resolution = LEDC_TIMER_13_BIT;
    timer.freq_hz = 1000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {};
    ch.gpio_num   = GPIO_NUM_45;
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel    = LEDC_CHANNEL_0;
    ch.intr_type  = LEDC_INTR_DISABLE;
    ch.timer_sel  = LEDC_TIMER_0;
    ch.duty       = 0;
    ch.hpoint     = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

void TFT_BL_SetBrightness(float brightness)
{
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    uint32_t duty = (uint32_t)(brightness * 8191);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}


LGFX_ESP32ST7789::LGFX_ESP32ST7789(void)
{

    // SPI配置
    {
        auto cfg = _bus_instance.config();

        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 3;

        cfg.freq_write = 27000000;
        cfg.freq_read  = 16000000;


        cfg.pin_mosi = 11;
        cfg.pin_miso = -1;
        cfg.pin_sclk = 12;
        cfg.pin_dc   = 9;


        _bus_instance.config(cfg);

        _panel_instance.setBus(&_bus_instance);
    }



    // ST7789配置
    {
        auto cfg = _panel_instance.config();


        cfg.pin_cs = -1;
        cfg.pin_rst = 8;
        cfg.pin_busy = -1;


        cfg.memory_width  = 240;
        cfg.memory_height = 240;


        cfg.panel_width  = 240;
        cfg.panel_height = 240;


        cfg.offset_x = 0;
        cfg.offset_y = 0;


        /*
         * 关键
         * ST7789很多240x240模块实际为BGR
         */
        cfg.rgb_order = false;


        cfg.bus_shared = false;


        _panel_instance.config(cfg);
    }


    setPanel(&_panel_instance);

}




void LGFX_ESP32ST7789::init()
{
    LGFX_Device::init();
    setSwapBytes(true);
    invertDisplay(true);  // 启用颜色反转（部分ST7789模块需要）
}





void LGFX_ESP32ST7789::reset()
{

    gpio_set_direction(
        GPIO_NUM_8,
        GPIO_MODE_OUTPUT
    );


    gpio_set_level(
        GPIO_NUM_8,
        0
    );


    vTaskDelay(
        pdMS_TO_TICKS(50)
    );


    gpio_set_level(
        GPIO_NUM_8,
        1
    );


    vTaskDelay(
        pdMS_TO_TICKS(150)
    );

}