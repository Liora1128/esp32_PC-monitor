#include "1_3TFT.h"

// ========== LEDC 背光 PWM 独立函数 ==========

// LEDC = LED Controller，ESP32 专用的高频 PWM 控制硬件
// 用于控制 TFT 屏幕背光的亮度（通过调节占空比实现）

void TFT_BL_Init(void)
{
    // ----- 定时器配置 -----
    ledc_timer_config_t timer = {};

    timer.speed_mode = LEDC_LOW_SPEED_MODE;  // 低速模式（省电，高速模式适合电机等场景）
    timer.timer_num  = LEDC_TIMER_0;          // 使用定时器 0（ESP32 有 4 个 LEDC 定时器可用）
    timer.duty_resolution = LEDC_TIMER_13_BIT; // PWM 分辨率 13bit = 2^13 = 8192 个等级（0~8191）
    timer.freq_hz = 1000;                     // PWM 频率 1000Hz（人眼看不到闪烁，需 >200Hz）
    timer.clk_cfg = LEDC_AUTO_CLK;            // 自动选择时钟源（通常是 APB 时钟 80MHz）

    ESP_ERROR_CHECK(ledc_timer_config(&timer));  // 宏：检查返回值，失败则打印错误并终止

    // ----- 通道配置 -----
    ledc_channel_config_t ch = {};

    ch.gpio_num   = GPIO_NUM_45;              // LEDC 输出引脚（接 TFT 背光控制引脚）
    ch.speed_mode = LEDC_LOW_SPEED_MODE;      // 需与定时器模式一致
    ch.channel    = LEDC_CHANNEL_0;           // 使用通道 0（ESP32 有 8 个 LEDC 通道）
    ch.intr_type  = LEDC_INTR_DISABLE;        // 禁用中断（不需要 PWM 周期中断）
    ch.timer_sel  = LEDC_TIMER_0;             // 绑定到定时器 0（决定频率和分辨率）
    ch.duty       = 0;                        // 初始占空比 0（全暗）
    ch.hpoint     = 0;                        // PWM 周期起始点，一般为 0

    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

// 设置背光亮度
// brightness: 0.0（最暗）~ 1.0（最亮）
// 注意：13bit 分辨率 = 8192 等级，所以 duty = brightness * 8191
void TFT_BL_SetBrightness(float brightness) {
    uint32_t duty = (uint32_t)(brightness * 8191);  // 将 0.0~1.0 映射到 0~8191
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);  // 写入 duty 值（待生效）
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);    // 立即更新 PWM 输出
}

// ========== LGFX_ESP32ST7789 类实现 ==========
// LGFX = Light and Graphics FX，一个轻量级的嵌入式图形库
// ST7789 = 常见的 1.3" TFT LCD 驱动芯片（240x240 分辨率）

LGFX_ESP32ST7789::LGFX_ESP32ST7789(void) {

    // ----- 第一部分：SPI 总线配置 -----
    // TFT 屏幕通过 SPI 协议与 ESP32 通信
    {
        auto cfg = _bus_instance.config();

        cfg.spi_host   = SPI2_HOST;           // 使用 SPI2 总线（ESP32 有 SPI1/2/3）
        cfg.spi_mode   = 3;                   // SPI 模式 3（CPOL=1, CPHA=1，最常用）
        cfg.freq_write = 27000000;            // 写数据频率 27MHz（TFT 写入速度）
        cfg.freq_read  = 16000000;            // 读数据频率 16MHz（TFT 读取速度，一般比写入慢）
        cfg.pin_mosi   = 11;                  // MOSI 引脚（主机输出从机输入）→ TFT DIN
        cfg.pin_miso   = -1;                   // MISO 引脚（主机输入从机输出），TFT 不需要读返回 → -1 禁用
        cfg.pin_sclk   = 12;                   // SCLK 引脚（SPI 时钟）
        cfg.pin_dc     = 9;                    // DC/RS 引脚（数据/命令切换脚）
        _bus_instance.config(cfg);
        _panel_instance.setBus(&_bus_instance);
    }

    // ----- 第二部分：面板（屏幕）参数配置 -----
    {
        auto cfg = _panel_instance.config();

        cfg.pin_cs   = -1;                    // CS 片选引脚，-1 = 软件控制（我们不用硬件 CS）
        cfg.pin_rst  = 8;                     // RESET 复位引脚（低电平复位屏幕）
        cfg.pin_busy = -1;                    // BUSY 引脚，-1 = 不用等待（有些屏幕需要）
        cfg.memory_width  = 240;              // 屏幕内部显存宽度（像素）
        cfg.memory_height = 240;              // 屏幕内部显存高度（像素）
        cfg.panel_width  = 240;               // 可视区域宽度（通常与显存一致）
        cfg.panel_height = 240;               // 可视区域高度（通常与显存一致）
        cfg.offset_x = 0;                     // X 方向偏移（部分屏幕需要调整起始坐标）
        cfg.offset_y = 0;                     // Y 方向偏移（部分屏幕需要调整起始坐标）
        cfg.rgb_order = false;                // 颜色字节顺序，false = RGB（大多数 TFT 用这个顺序）
        cfg.bus_shared = false;               // 总线是否共享，false = 独占（不与其他设备共用 SPI）
        _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
}

// 复位屏幕
void LGFX_ESP32ST7789::reset(void) {
    gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);  // 设置为输出模式
    gpio_set_level(GPIO_NUM_8, 0);                    // 拉低复位（屏幕复位需要低电平）
    vTaskDelay(pdMS_TO_TICKS(50));                    // 等待 50ms（满足复位低电平时间要求）
    gpio_set_level(GPIO_NUM_8, 1);                   // 拉高，结束复位
    vTaskDelay(pdMS_TO_TICKS(150));                   // 等待 150ms（等待屏幕初始化完成）
}

// 初始化屏幕
void LGFX_ESP32ST7789::init(void) {
    // 调用父类 LGFX_Device 的初始化
    // 内部会完成：总线初始化、屏幕驱动初始化、默认参数设置
    LGFX_Device::init();
}
