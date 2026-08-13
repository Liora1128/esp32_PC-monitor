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
#include "ProvisionUI.h"

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
// BOOT 长按 5 秒清除 Wi-Fi
// ============================================================

static void boot_hold_task(void *arg)
{
    (void)arg;

    KeyEvent ev;

    while (1) {

        KeyState st =
            My_Button_GetEvent(
                &ev,
                portMAX_DELAY
            );

        if (st != KEY_HOLD_5S)
            continue;

        if (ev.pin != GPIO_NUM_1)
            continue;

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
}

// ============================================================
// 网络后台任务
// ============================================================
//
// 非常重要：
//
// 原来的代码直接：
//
//     NetSync_StartBackground();
//
// 这个函数在没有 Wi-Fi 时会进入
// WifiProvision_StartAP()，而 StartAP() 本身一直运行。
//
// 所以 main task 被卡住，LVGL 也就停止。
// 最终屏幕就一直黑。
//
// 现在把它放到独立 FreeRTOS task。
// main task 专门负责 LVGL。
// ============================================================

static void network_task(void *arg)
{
    (void)arg;

    ESP_LOGI(
        "main",
        "network task started"
    );

    NetSync_StartBackground();

    ESP_LOGI(
        "main",
        "network task finished"
    );

    vTaskDelete(NULL);
}

// ============================================================
// Dashboard 切换
// ============================================================

static bool s_dashboard_started = false;

static void try_switch_to_dashboard()
{
    if (s_dashboard_started)
        return;

    NetSync_State state =
        NetSync_GetState();

    if (state != NETSYNC_STATE_READY)
        return;

    ESP_LOGI(
        "main",
        "network ready -> starting Dashboard"
    );

    // 删除配网页面
    lv_obj_clean(
        lv_scr_act()
    );

    Dashboard_Start();

    s_dashboard_started = true;
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

    ESP_LOGI(
        "main",
        "display initialized"
    );

    // --------------------------------------------------------
    // 2. 创建配网/启动动画
    // --------------------------------------------------------

    ProvisionUI_Start();

    // --------------------------------------------------------
    // 3. BOOT 5 秒监听
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
        "BOOT 5s hold handler armed"
    );

    // --------------------------------------------------------
    // 4. 网络后台任务
    // --------------------------------------------------------
    //
    // 注意：
    // 这里绝对不能直接调用
    //
    //     NetSync_StartBackground();
    //
    // 否则配网时又会阻塞 LVGL。
    //
    // --------------------------------------------------------

    BaseType_t ret =
        xTaskCreate(
            network_task,
            "network",
            8192,
            NULL,
            5,
            NULL
        );

    if (ret != pdPASS) {

        ESP_LOGE(
            "main",
            "failed to create network task"
        );
    }

    // --------------------------------------------------------
    // 5. LVGL 主循环
    // --------------------------------------------------------

    while (1) {

        // ----------------------------------------------------
        // 网络正常后切换 Dashboard
        // ----------------------------------------------------

        try_switch_to_dashboard();

        // ----------------------------------------------------
        // 配网动画
        // ----------------------------------------------------

        if (!s_dashboard_started) {

            NetSync_State state =
                NetSync_GetState();

            if (state == NETSYNC_STATE_PROVISIONING)
            {
                // 没有保存 Wi-Fi：
                // 真正进入配网模式后才显示
                // PCMonitor-Setup
                ProvisionUI_ShowWaiting();
            }
            else if (state == NETSYNC_STATE_CONNECTING)
            {
                // 已经有保存的 Wi-Fi：
                // 重启以后直接显示 Connecting
                ProvisionUI_ShowConnecting();
            }
            else if (state == NETSYNC_STATE_PROVISION_SUCCESS)
            {
                // Wi-Fi 验证成功，正在显示成功页
                ProvisionUI_ShowSuccess();
            }
            else if (state == NETSYNC_STATE_STARTING)
            {
                // 刚开机，暂时什么都不切换。
                // 保持 ProvisionUI_Start() 的 Starting...
            }

            ProvisionUI_Update();
        }

        // ----------------------------------------------------
        // LVGL
        // ----------------------------------------------------

        lv_tick_inc(50);

        lv_timer_handler();

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}