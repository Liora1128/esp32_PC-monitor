// ESP32S3 watch firmware - top-level flow.
// All heavy lifting lives in /lib modules; this file is just bootstrap +
// the LVGL tick loop.

#include <time.h>
#include <sys/time.h>

#include <LovyanGFX.hpp>
#include "1_3TFT.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

#include "Clock_UI.h"
#include "Menu_System.h"
#include "NetSync.h"
#include "Weather.h"
#include "My_Button.h"
#include "lib\RtosTasks\freertos.h"
#include "Dashboard.h"

#include "App_Music.h"
#include "App_Games.h"
#include "App_Flashlight.h"
#include "App_Weather.h"
#include "App_Settings.h"
#include "App_Stopwatch.h"

// ---------- LVGL display plumbing ----------

static LGFX_ESP32ST7789 s_tft;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t s_buf[2][240 * 10];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    s_tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(drv);
}

static void lvgl_init(void)
{
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_buf[0], s_buf[1], 240 * 10);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = 240;
    drv.ver_res = 240;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&drv);
}

// Seed the system clock so the UI has a sensible time before NTP arrives.
static void seed_clock(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon  = 8 - 1;
    t.tm_mday = 7;
    t.tm_hour = 12;
    t.tm_min  = 0;
    t.tm_sec  = 0;
    struct timeval tv = { .tv_sec = mktime(&t), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

// ---------- main ----------

extern "C" void app_main(void)
{
    s_tft.reset();
    s_tft.init();
    TFT_BL_Init();
    TFT_BL_SetBrightness(1.0);
    My_Button_Init();

    seed_clock();
    lvgl_init();

    // Build the PC-monitor Dashboard UI. It also starts its own UDP
    // listener (port 9999) that updates the data fields from JSON
    // pushed by the PC.
    Dashboard_Start();

    // Let the display fully render before background work starts.
    vTaskDelay(pdMS_TO_TICKS(3000));
    NetSync_StartBackground();
    BtnPoll_StartBackground();

    // Give NetSync ~25 s to finish its first weather fetch, then start
    // the periodic refresher. Without this delay, the two weather tasks
    // overlap and conflict over the HTTP stack / shared buffer.
    vTaskDelay(pdMS_TO_TICKS(25000));
    Weather_StartPeriodic(30 * 60 * 1000);

    while (1) {
        lv_tick_inc(50);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}