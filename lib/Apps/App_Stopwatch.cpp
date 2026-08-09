// Stopwatch: OK starts/stops, LEFT/RIGHT clear. Time is rendered as
// MM:SS.d with a 1-Hz LVGL timer.

#include "App_Stopwatch.h"
#include "Menu_System.h"

#include <lvgl.h>
#include <time.h>

static bool        s_running  = false;
static uint32_t    s_start_ms = 0;
static uint32_t    s_accum_ms = 0;
static lv_obj_t   *s_time_label = NULL;
static lv_timer_t *s_timer     = NULL;

static lv_obj_t *s_status_label = NULL;

static void format_time(char *out, size_t outlen, uint32_t ms)
{
    uint32_t total_sec = ms / 1000;
    uint32_t mm = total_sec / 60;
    uint32_t ss = total_sec % 60;
    uint32_t tenths = (ms % 1000) / 100;
    lv_snprintf(out, outlen, "%02lu:%02lu.%lu",
                (unsigned long)mm, (unsigned long)ss, (unsigned long)tenths);
}

static uint32_t elapsed_ms(void)
{
    if (!s_running) return s_accum_ms;
    return s_accum_ms + (uint32_t)((lv_tick_get() - s_start_ms) / 1000ULL * 1000ULL);
    // Note: lv_tick_get() returns ms; this is approximate but sufficient.
}

static void stopwatch_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_time_label) return;
    char buf[24];
    format_time(buf, sizeof(buf), elapsed_ms());
    lv_label_set_text(s_time_label, buf);
    if (s_status_label) {
        lv_label_set_text(s_status_label, s_running ? "Running" : "Stopped");
    }
}

static void stopwatch_enter(void)
{
    s_running  = false;
    s_accum_ms = 0;
    s_start_ms = lv_tick_get();

    lv_obj_t *body = MenuSystem_GetAppBody();
    MenuSystem_ShowAppTitle(true);

    s_time_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_time_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_time_label, lv_color_white(), 0);
    lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, -10);

    s_status_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_make(140, 200, 180), 0);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    s_timer = lv_timer_create(stopwatch_timer_cb, 100, NULL);

    stopwatch_timer_cb(s_timer);
}

static void stopwatch_exit(void)
{
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_time_label   = NULL;
    s_status_label = NULL;
    s_running      = false;
    s_accum_ms     = 0;
}

// on_key: OK toggles run/stop (consume OK so menu layer won't exit).
//        LEFT clears, RIGHT no-op.
static bool stopwatch_on_key(menu_key_t key)
{
    if (key == MENU_KEY_OK) {
        if (s_running) {
            s_accum_ms = elapsed_ms();
            s_running = false;
        } else {
            s_start_ms = lv_tick_get();
            s_running  = true;
        }
        stopwatch_timer_cb(NULL);
        return true;  // consumed OK
    }
    if (key == MENU_KEY_LEFT) {
        s_running  = false;
        s_accum_ms = 0;
        stopwatch_timer_cb(NULL);
    }
    return false;
}

const menu_app_t App_Stopwatch = {
    .name   = "Stopwatch",
    .color  = LV_COLOR_MAKE(120, 220, 140),
    .enter  = stopwatch_enter,
    .exit   = stopwatch_exit,
    .on_key = stopwatch_on_key,
};