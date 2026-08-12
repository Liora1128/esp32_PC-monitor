// ESP32S3 monitor firmware - top-level flow.
// Display/dashboard UI is handled by Dashboard.
// Network provisioning, Wi-Fi connection and UDP business logic
// are handled by NetSync / wifi_provision.

#include <LovyanGFX.hpp>
#include "1_3TFT.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

#include "NetSync.h"
#include "wifi_provision.h"
#include "My_Button.h"
#include "lib/RtosTasks/freertos.h"
#include "Dashboard.h"

// ============================================================
// LVGL display
// ============================================================

static LGFX_ESP32ST7789 s_tft;

static lv_disp_draw_buf_t s_draw_buf;

static lv_color_t s_buf[2][240 * 10];

static void flush_cb(
    lv_disp_drv_t *drv,
    const lv_area_t *area,
    lv_color_t *color_p)
{
    uint32_t w =
        area->x2 - area->x1 + 1;

    uint32_t h =
        area->y2 - area->y1 + 1;

    s_tft.pushImageDMA(
        area->x1,
        area->y1,
        w,
        h,
        (uint16_t *)&color_p->full
    );

    lv_disp_flush_ready(drv);
}

static void lvgl_init(void)
{
    lv_init();

    lv_disp_draw_buf_init(
        &s_draw_buf,
        s_buf[0],
        s_buf[1],
        240 * 10
    );

    static lv_disp_drv_t drv;

    lv_disp_drv_init(&drv);

    drv.hor_res = 240;
    drv.ver_res = 240;

    drv.flush_cb = flush_cb;
    drv.draw_buf = &s_draw_buf;

    lv_disp_drv_register(&drv);
}

// ============================================================
// BOOT 长按监控
//
// GPIO1：
//   按下 = 低电平
//   松开 = 高电平
//
// 长按 5 秒：
//   清除保存的 Wi-Fi
//   重启
//   重启后自动进入配网模式
// ============================================================

static void boot_button_task(void *arg)
{
    (void)arg;

    const uint32_t HOLD_MS = 5000;
    const uint32_t POLL_MS = 50;

    uint32_t held_ms = 0;

    const gpio_num_t BOOT =
        GPIO_NUM_1;

    while (1) {

        if (gpio_get_level(BOOT) == 0) {

            held_ms += POLL_MS;

            if (held_ms >= HOLD_MS) {

                ESP_LOGW(
                    "main",
                    "BOOT held for 5 seconds"
                );

                ESP_LOGW(
                    "main",
                    "clearing saved Wi-Fi credentials"
                );

                WifiProvision_ClearCredentials();

                vTaskDelay(
                    pdMS_TO_TICKS(100)
                );

                ESP_LOGW(
                    "main",
                    "restarting into provisioning mode"
                );

                esp_restart();
            }

        } else {

            held_ms = 0;
        }

        vTaskDelay(
            pdMS_TO_TICKS(POLL_MS)
        );
    }
}

// ============================================================
// main
// ============================================================

extern "C" void app_main(void)
{
    // --------------------------------------------------------
    // 1. 初始化屏幕
    // --------------------------------------------------------

    s_tft.reset();

    s_tft.init();

    TFT_BL_Init();

    TFT_BL_SetBrightness(1.0);

    My_Button_Init();

    lvgl_init();

    // --------------------------------------------------------
    // 2. 创建监控 Dashboard
    //
    // Dashboard 本身不创建 UDP socket。
    // UDP 由 NetSync 接收：
    //
    // PC
    //  ↓ UDP 9999
    // NetSync
    //  ↓
    // NetSync_Data
    //  ↓
    // Dashboard
    // --------------------------------------------------------

    Dashboard_Start();

    // 给 LVGL / LCD 一点初始化时间
    vTaskDelay(
        pdMS_TO_TICKS(300)
    );

    // --------------------------------------------------------
    // 3. 配置 BOOT 按键
    //
    // GPIO1 上拉输入
    // 按下接地 = 0
    // 长按 5 秒清除 Wi-Fi 并重启
    // --------------------------------------------------------

    const gpio_num_t BOOT =
        GPIO_NUM_1;

    gpio_config_t io = {
        .pin_bit_mask =
            (1ULL << BOOT),

        .mode =
            GPIO_MODE_INPUT,

        .pull_up_en =
            GPIO_PULLUP_ENABLE,

        .pull_down_en =
            GPIO_PULLDOWN_DISABLE,

        .intr_type =
            GPIO_INTR_DISABLE,
    };

    esp_err_t gpio_err =
        gpio_config(&io);

    if (gpio_err != ESP_OK) {

        ESP_LOGE(
            "main",
            "BOOT GPIO config failed: %s",
            esp_err_to_name(gpio_err)
        );

    } else {

        ESP_LOGI(
            "main",
            "BOOT button configured on GPIO1"
        );
    }

    // 启动 BOOT 长按检测任务
    xTaskCreate(
        boot_button_task,
        "btn_watch",
        2048,
        NULL,
        1,
        NULL
    );

    // --------------------------------------------------------
    // 4. 启动网络系统
    //
    // NetSync_StartBackground() 会根据 NVS 状态自动选择：
    //
    // A. 没有 Wi-Fi 配置：
    //
    //      AP
    //       ↓
    //      DHCP
    //       ↓
    //      DNS
    //       ↓
    //      Captive Portal
    //       ↓
    //      手机配网
    //       ↓
    //      保存 NVS
    //       ↓
    //      esp_restart()
    //
    // B. 已经有 Wi-Fi：
    //
    //      读取 NVS
    //       ↓
    //      连接 Wi-Fi
    //       ↓
    //      启动 UDP 9999
    //      启动 UDP 9998
    //       ↓
    //      正常监控模式
    //
    // 注意：
    // 配网模式下 NetSync_StartBackground()
    // 会停留在 WifiProvision_StartAP() 中，
    // 直到成功配置后重启。
    // --------------------------------------------------------

    ESP_LOGI(
        "main",
        "starting network subsystem"
    );

    NetSync_StartBackground();

    // --------------------------------------------------------
    // 5. LVGL 主循环
    //
    // 正常模式下：
    //
    //      NetSync 收 UDP 数据
    //             ↓
    //        NetSync_Data
    //             ↓
    //        Dashboard tick_cb
    //             ↓
    //           LVGL
    //
    // 配网模式下：
    //      不会执行到这里，因为
    //      WifiProvision_StartAP() 会一直运行，
    //      成功后直接 esp_restart()
    // --------------------------------------------------------

    while (1) {

        lv_tick_inc(50);

        lv_timer_handler();

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}