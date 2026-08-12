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
// BOOT 长按 5s 清 WiFi 任务
//
// 这里不再自己做 GPIO 轮询 / ISR，全部依赖 My_Button 中断驱动：
//   - GPIO1 (BOOT) 已经被 My_Button_Init() 装好了双边沿 ISR
//   - 按住 5s 后 My_Button 会推一个 KEY_HOLD_5S 事件到队列
//   - 本任务阻塞读，拿到这个事件就清 NVS 凭据 + esp_restart()
//
// CPU 不轮询，My_Button 的 esp_timer 5s 触发后自动唤醒这里。
// ============================================================

static void boot_hold_task(void *arg)
{
    (void)arg;

    KeyEvent ev;
    while (1) {
        KeyState st = My_Button_GetEvent(&ev, portMAX_DELAY);
        if (st != KEY_HOLD_5S) continue;

        // 只对 BOOT (GPIO1) 关心，其它 pin 上的 KEY_HOLD_5S 忽略
        // （理论上三个 pin 都能触发，但只有 BOOT 配 NVS 操作）。
        if (ev.pin != GPIO_NUM_1) continue;

        ESP_LOGW("main", "BOOT held for 5 seconds");
        ESP_LOGW("main", "clearing saved Wi-Fi credentials");

        WifiProvision_ClearCredentials();

        vTaskDelay(pdMS_TO_TICKS(100));

        ESP_LOGW("main", "restarting into provisioning mode");

        esp_restart();
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
    // 3. 启动 BOOT 5s 长按监听任务
    //
    // 5s 长按的 GPIO 边沿检测、定时全部由 My_Button 中断模块完成，
    // 这里只需要一个轻量 task 订阅 KEY_HOLD_5S 事件去清 WiFi。
    //
    // 注意：My_Button_Init() 已经在步骤 1 里把 GPIO 1 装好了
    // 输入上拉 + 双边沿 ISR + 5s esp_timer, 这里不需要再做任何
    // GPIO 配置。
    // --------------------------------------------------------

    xTaskCreate(
        boot_hold_task,
        "boot_hold",
        2048,
        NULL,
        1,
        NULL
    );

    ESP_LOGI(
        "main",
        "BOOT 5s hold handler armed (subscribed to My_Button KEY_HOLD_5S)"
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