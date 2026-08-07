#include <LovyanGFX.hpp>
#include "1_3TFT.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"

static LGFX_ESP32ST7789 tft;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[2][240 * 10];

static lv_obj_t * hour_label;
static lv_obj_t * min_label;
static lv_obj_t * sec_label;
static lv_obj_t * date_label;
static lv_obj_t * ampm_label;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

void update_clock()
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    int hour = t->tm_hour;
    int min = t->tm_min;
    int sec = t->tm_sec;

    // Format hour
    static char h_buf[3];
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;
    h_buf[0] = '0' + hour / 10;
    h_buf[1] = '0' + hour % 10;
    h_buf[2] = '\0';

    // Format min/sec
    static char m_buf[3], s_buf[3];
    m_buf[0] = '0' + min / 10;
    m_buf[1] = '0' + min % 10;
    m_buf[2] = '\0';
    s_buf[0] = '0' + sec / 10;
    s_buf[1] = '0' + sec % 10;
    s_buf[2] = '\0';

    lv_label_set_text(hour_label, h_buf);
    lv_label_set_text(min_label, m_buf);
    lv_label_set_text(sec_label, s_buf);

    // AM/PM
    static char ampm[3];
    t->tm_hour >= 12 ? strcpy(ampm, "PM") : strcpy(ampm, "AM");
    lv_label_set_text(ampm_label, ampm);

    // Date
    static const char *days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    static const char *months[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
    static char date_buf[32];
    sprintf(date_buf, "%s  %s  %02d", days[t->tm_wday], months[t->tm_mon], t->tm_mday);
    lv_label_set_text(date_label, date_buf);
}

void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_clock();
}

extern "C"
void app_main()
{
    tft.reset();
    tft.init();
    TFT_BL_Init();
    TFT_BL_SetBrightness(1.0);

    setenv("TZ", "CST-8", 1);
    tzset();

    struct tm t;
    t.tm_year = 2026 - 1900;
    t.tm_mon = 7;
    t.tm_mday = 7;
    t.tm_hour = 12;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = 0;
    time_t ts = mktime(&t);
    struct timeval tv = { .tv_sec = ts, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf[0], buf[1], 240 * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ===== Background =====
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(5, 5, 15), 0);

    // ===== Top decoration circle =====
    lv_obj_t * top_circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_circle, 120, 120);
    lv_obj_set_style_radius(top_circle, 60, 0);
    lv_obj_set_style_bg_color(top_circle, lv_color_make(20, 20, 50), 0);
    lv_obj_set_style_border_width(top_circle, 2, 0);
    lv_obj_set_style_border_color(top_circle, lv_color_make(0, 180, 255), 0);
    lv_obj_align(top_circle, LV_ALIGN_TOP_MID, 0, 5);

    // ===== Clock panel =====
    lv_obj_t * clock_panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(clock_panel, 220, 110);
    lv_obj_set_style_bg_color(clock_panel, lv_color_make(10, 10, 25), 0);
    lv_obj_set_style_border_width(clock_panel, 1, 0);
    lv_obj_set_style_border_color(clock_panel, lv_color_make(0, 120, 200), 0);
    lv_obj_set_style_radius(clock_panel, 12, 0);
    lv_obj_align(clock_panel, LV_ALIGN_CENTER, 0, -15);

    // Hour
    hour_label = lv_label_create(clock_panel);
    lv_label_set_text(hour_label, "00");
    lv_obj_set_style_text_font(hour_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(hour_label, lv_color_make(0, 220, 255), 0);
    lv_obj_align(hour_label, LV_ALIGN_CENTER, -60, 0);

    // Colon
    lv_obj_t * col1 = lv_label_create(clock_panel);
    lv_label_set_text(col1, ":");
    lv_obj_set_style_text_font(col1, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(col1, lv_color_make(255, 140, 0), 0);
    lv_obj_align(col1, LV_ALIGN_CENTER, -20, 0);

    // Minute
    min_label = lv_label_create(clock_panel);
    lv_label_set_text(min_label, "00");
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(min_label, lv_color_make(0, 220, 255), 0);
    lv_obj_align(min_label, LV_ALIGN_CENTER, 15, 0);

    // Colon
    lv_obj_t * col2 = lv_label_create(clock_panel);
    lv_label_set_text(col2, ":");
    lv_obj_set_style_text_font(col2, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(col2, lv_color_make(255, 140, 0), 0);
    lv_obj_align(col2, LV_ALIGN_CENTER, 50, 0);

    // Second
    sec_label = lv_label_create(clock_panel);
    lv_label_set_text(sec_label, "00");
    lv_obj_set_style_text_font(sec_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(sec_label, lv_color_make(255, 140, 0), 0);
    lv_obj_align(sec_label, LV_ALIGN_CENTER, 85, 0);

    // AM/PM label
    ampm_label = lv_label_create(clock_panel);
    lv_label_set_text(ampm_label, "PM");
    lv_obj_set_style_text_font(ampm_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ampm_label, lv_color_make(0, 180, 255), 0);
    lv_obj_align(ampm_label, LV_ALIGN_CENTER, 0, -42);

    // ===== Date =====
    date_label = lv_label_create(lv_scr_act());
    lv_label_set_text(date_label, "FRI  AUG  07");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(date_label, lv_color_make(150, 150, 180), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 75);

    // ===== Bottom dots decoration =====
    for (int i = 0; i < 5; i++) {
        lv_obj_t * dot = lv_obj_create(lv_scr_act());
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_color(dot, lv_color_make(0, 120, 200), 0);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (i - 2) * 15, -15);
    }

    update_clock();
    lv_timer_create(clock_timer_cb, 1000, NULL);

    printf("LVGL Digital Clock started\n");

    while (1) {
        lv_tick_inc(10);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
