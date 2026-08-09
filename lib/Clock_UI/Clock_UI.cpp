#include "Clock_UI.h"

#include <time.h>
#include <string.h>
#include <cstdio>

// --- module state ---
static lv_obj_t *s_hour_label;
static lv_obj_t *s_min_label;
static lv_obj_t *s_sec_label;
static lv_obj_t *s_ampm_label;
static lv_obj_t *s_date_label;

// ---- helpers ----

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

// Refresh the time/date fields from current system time.
static void refresh_time(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    int hour = t->tm_hour;
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;

    static const char *days[]   = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    static const char *months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                   "JUL","AUG","SEP","OCT","NOV","DEC"};

    char h_buf[3], m_buf[3], s_buf[3], ap_buf[3], d_buf[24];
    h_buf[0] = '0' + hour / 10;
    h_buf[1] = '0' + hour % 10;
    h_buf[2] = '\0';
    m_buf[0] = '0' + t->tm_min / 10;
    m_buf[1] = '0' + t->tm_min % 10;
    m_buf[2] = '\0';
    s_buf[0] = '0' + t->tm_sec / 10;
    s_buf[1] = '0' + t->tm_sec % 10;
    s_buf[2] = '\0';
    strcpy(ap_buf, t->tm_hour >= 12 ? "PM" : "AM");
    snprintf(d_buf, sizeof(d_buf), "%s  %s  %02d",
             days[t->tm_wday], months[t->tm_mon], t->tm_mday);

    lv_label_set_text(s_hour_label, h_buf);
    lv_label_set_text(s_min_label,  m_buf);
    lv_label_set_text(s_sec_label,  s_buf);
    lv_label_set_text(s_ampm_label, ap_buf);
    lv_label_set_text(s_date_label, d_buf);
}

// ---- public API ----

void ClockUI_Build(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(5, 5, 15), 0);

    // top decoration circle
    lv_obj_t *top_circle = lv_obj_create(lv_scr_act());
    lv_obj_set_size(top_circle, 120, 120);
    lv_obj_set_style_radius(top_circle, 60, 0);
    lv_obj_set_style_bg_color(top_circle, lv_color_make(20, 20, 50), 0);
    lv_obj_set_style_border_width(top_circle, 2, 0);
    lv_obj_set_style_border_color(top_circle, lv_color_make(0, 180, 255), 0);
    lv_obj_align(top_circle, LV_ALIGN_TOP_MID, 0, 5);

    // clock panel container
    lv_obj_t *panel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(panel, 220, 80);
    lv_obj_set_style_bg_color(panel, lv_color_make(10, 10, 25), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_make(0, 120, 200), 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, -10);

    s_hour_label = make_label(panel, &lv_font_montserrat_32,
                              lv_color_make(0, 220, 255), "00");
    lv_obj_align(s_hour_label, LV_ALIGN_CENTER, -65, 0);

    lv_obj_t *col1 = make_label(panel, &lv_font_montserrat_32,
                                lv_color_make(255, 140, 0), ":");
    lv_obj_align(col1, LV_ALIGN_CENTER, -28, 0);

    s_min_label = make_label(panel, &lv_font_montserrat_32,
                             lv_color_make(0, 220, 255), "00");
    lv_obj_align(s_min_label, LV_ALIGN_CENTER, 10, 0);

    lv_obj_t *col2 = make_label(panel, &lv_font_montserrat_32,
                                lv_color_make(255, 140, 0), ":");
    lv_obj_align(col2, LV_ALIGN_CENTER, 47, 0);

    s_sec_label = make_label(panel, &lv_font_montserrat_32,
                             lv_color_make(255, 140, 0), "00");
    lv_obj_align(s_sec_label, LV_ALIGN_CENTER, 70, 0);

    s_ampm_label = make_label(panel, &lv_font_montserrat_14,
                              lv_color_make(0, 180, 255), "PM");
    lv_obj_align(s_ampm_label, LV_ALIGN_CENTER, 0, -40);

    // date (outside the panel)
    s_date_label = make_label(lv_scr_act(), &lv_font_montserrat_14,
                              lv_color_make(150, 150, 180), "FRI  AUG  07");
    lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 75);

    // bottom dots decoration
    for (int i = 0; i < 5; i++) {
        lv_obj_t *dot = lv_obj_create(lv_scr_act());
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, 3, 0);
        lv_obj_set_style_bg_color(dot, lv_color_make(0, 120, 200), 0);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (i - 2) * 15, -15);
    }
}

void ClockUI_RefreshTime(void)
{
    refresh_time();
}

static void auto_refresh_cb(lv_timer_t *t)
{
    (void)t;
    refresh_time();
}

void ClockUI_StartAutoRefresh(uint32_t period_ms)
{
    refresh_time();
    lv_timer_create(auto_refresh_cb, period_ms, NULL);
}