#include "Dashboard.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "lvgl.h"
#include "My_Button.h"
#include "NetSync.h"

// PC MONITOR DASHBOARD - single overview page.
//   top-left:   CPU ring + percent
//   top-right:  UPTIME / CPU FREQ card
//   middle:     RAM bar + percent
//   bottom:     UP / DOWN speed cards

// ---------- Palette ----------
#define COL_BG_DEEP    LV_COLOR_MAKE(8,  12,  24)
#define COL_BG_TOP     LV_COLOR_MAKE(18, 24,  44)
#define COL_CARD       LV_COLOR_MAKE(28, 36,  58)
#define COL_TEXT       LV_COLOR_MAKE(240, 245, 255)
#define COL_DIM        LV_COLOR_MAKE(140, 150, 175)
#define COL_ACCENT     LV_COLOR_MAKE(0,   229, 255)
#define COL_GREEN      LV_COLOR_MAKE(16,  200, 130)
#define COL_PURPLE     LV_COLOR_MAKE(168, 85,  247)
#define COL_ORANGE     LV_COLOR_MAKE(251, 146, 60)
#define COL_RED        LV_COLOR_MAKE(244, 63,  94)

// ---------- Shared widgets ----------
static lv_obj_t *s_root;

// ---------- Overview widgets ----------
static lv_obj_t *s_cpu_arc;
static lv_obj_t *s_cpu_pct_label;
static lv_obj_t *s_ram_bar;
static lv_obj_t *s_ram_pct_label;
static lv_obj_t *s_up_speed_label;
static lv_obj_t *s_dn_speed_label;
static lv_obj_t *s_cpu_temp_label;
static lv_obj_t *s_uptime_home_label;

static lv_obj_t *s_top_info_title;

static bool s_show_cpu_temp = false;
static uint8_t s_switch_seconds = 0;

static uint8_t s_has_cpu_temp = 0;
static uint8_t s_has_uptime = 0;
static uint8_t s_cpu_temp = 0;

// ============================================================================
//  Helpers
// ============================================================================
static lv_obj_t *mk_lbl(lv_obj_t *parent, const lv_font_t *font,
                        lv_color_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static lv_obj_t *mk_card(lv_obj_t *parent, int w, int h,
                         lv_color_t border, int border_opa)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, COL_CARD, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_70, 0);
    lv_obj_set_style_border_color(c, border, 0);
    lv_obj_set_style_border_opa(c, border_opa, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 12, 0);
    lv_obj_set_style_pad_all(c, 6, 0);
    return c;
}

// Format a bytes/sec value as "1.23 MB/s" or "456 KB/s".
static void fmt_speed(char *out, size_t n, uint32_t bps)
{
    if (bps >= 1024U * 1024U) {
        uint32_t mbps_x100 = (bps * 100U) / (1024U * 1024U);
        uint32_t whole = mbps_x100 / 100U;
        uint32_t frac  = mbps_x100 % 100U;
        lv_snprintf(out, n, "%lu.%02lu MB/s",
                    (unsigned long)whole, (unsigned long)frac);
    } else if (bps >= 1024U) {
        uint32_t kbps_x10 = (bps * 10U) / 1024U;
        uint32_t whole = kbps_x10 / 10U;
        uint32_t frac  = kbps_x10 % 10U;
        lv_snprintf(out, n, "%lu.%lu KB/s",
                    (unsigned long)whole, (unsigned long)frac);
    } else {
        lv_snprintf(out, n, "%lu B/s", (unsigned long)bps);
    }
}

// "1d 18:32" / "18:32" / "0:32"
static void fmt_uptime_days(char *out, size_t n, uint32_t sec)
{
    uint32_t d = sec / 86400;
    uint32_t h = (sec % 86400) / 3600;
    uint32_t m = (sec % 3600) / 60;
    if (d > 0) {
        lv_snprintf(out, n, "%lud %02lu:%02lu",
                    (unsigned long)d, (unsigned long)h, (unsigned long)m);
    } else if (h > 0) {
        lv_snprintf(out, n, "%lu:%02lu", (unsigned long)h, (unsigned long)m);
    } else {
        lv_snprintf(out, n, "0:%02lu", (unsigned long)m);
    }
}

// ============================================================================
//  Static display state (used until first valid PC packet arrives)
// ============================================================================
static uint8_t  s_cpu       = 0;
static uint16_t s_cpu_freq  = 0;   // MHz * 100
static uint8_t  s_ram       = 0;
static uint32_t s_uptime    = 0;
static uint32_t s_up        = 0;
static uint32_t s_down      = 0;
static uint8_t  s_have_data = 0;

// ============================================================================
//  Chrome + page
// ============================================================================
static void build_chrome(void)
{
    lv_obj_set_style_bg_color(lv_scr_act(), COL_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
}

static void build_page_system(void)
{
    lv_obj_t *p = lv_obj_create(s_root);
    lv_obj_set_size(p, 240, 196);
    lv_obj_align(p, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_size(p, 240, 224);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);

    // ---- CPU ring (top-left) ----
    s_cpu_arc = lv_arc_create(p);
    lv_obj_set_size(s_cpu_arc, 120, 120);
    lv_obj_align(s_cpu_arc, LV_ALIGN_TOP_LEFT, 6, 0);
    lv_arc_set_range(s_cpu_arc, 0, 100);
    lv_arc_set_bg_angles(s_cpu_arc, 135, 45);
    lv_arc_set_value(s_cpu_arc, s_cpu);
    lv_arc_set_angles(s_cpu_arc, 135, 135 + (270 * s_cpu) / 100);
    lv_obj_set_style_arc_color(s_cpu_arc, lv_color_make(20, 28, 50), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_cpu_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_cpu_arc, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_cpu_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_cpu_arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(s_cpu_arc, NULL, LV_PART_KNOB);

    char buf[16];
    lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_cpu);
    s_cpu_pct_label = mk_lbl(p, &lv_font_montserrat_48, COL_TEXT, buf);
    // 设置 label 宽度等于圆弧内径，并把文字在 label 内水平居中
    lv_obj_set_size(s_cpu_pct_label, 110, LV_SIZE_CONTENT);
    lv_label_set_long_mode(s_cpu_pct_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_cpu_pct_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(s_cpu_pct_label, s_cpu_arc, LV_ALIGN_CENTER, 0, -4);
    {
        lv_obj_t *cpu_cap = mk_lbl(p, &lv_font_montserrat_14, COL_DIM, "CPU %");
        lv_obj_set_size(cpu_cap, 110, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(cpu_cap, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(cpu_cap, s_cpu_arc, LV_ALIGN_CENTER, 0, 32);
    }

    // ---- Top-right: UPTIME + CPU FREQ ----
    lv_obj_t *freq_card = mk_card(p, 108, 108, COL_ACCENT, LV_OPA_40);
    lv_obj_align(freq_card, LV_ALIGN_TOP_RIGHT, -6, 0);

    {
        s_top_info_title = mk_lbl(freq_card, &lv_font_montserrat_14, COL_PURPLE, "UPTIME");
        // 此处修改uptime标签位置
        lv_obj_align(s_top_info_title, LV_ALIGN_TOP_LEFT, 6, 0);

        s_uptime_home_label = mk_lbl(freq_card, &lv_font_montserrat_16, COL_TEXT, "--");
        lv_obj_align(s_uptime_home_label, LV_ALIGN_TOP_MID, 0, 20);
    }

    {
        lv_obj_t *sep = lv_obj_create(freq_card);
        lv_obj_set_size(sep, 90, 1);
        lv_obj_align(sep, LV_ALIGN_CENTER, 0, 4);
        lv_obj_set_style_bg_color(sep, lv_color_make(40, 50, 80), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
    }

    {
        lv_obj_align(mk_lbl(freq_card, &lv_font_montserrat_14, COL_ACCENT, "CPU FREQ"),
                     LV_ALIGN_BOTTOM_LEFT, 6, -34);

        // s_cpu_freq is GHz * 100 (e.g. 272 -> "2.72")
        lv_snprintf(buf, sizeof(buf), "%lu.%02lu",
                    (unsigned long)(s_cpu_freq / 100),
                    (unsigned long)(s_cpu_freq % 100));
        s_cpu_temp_label = mk_lbl(freq_card, &lv_font_montserrat_16, COL_TEXT, buf);
        lv_obj_align(s_cpu_temp_label, LV_ALIGN_BOTTOM_MID, -12, -10);
        lv_obj_align(mk_lbl(freq_card, &lv_font_montserrat_14, COL_DIM, "GHz"),
                     LV_ALIGN_BOTTOM_RIGHT, -6, -8);
    }

    // ---- Middle: RAM bar ----
    lv_obj_t *ram_card = mk_card(p, 228, 38, COL_ACCENT, LV_OPA_40);
    lv_obj_align(ram_card, LV_ALIGN_TOP_MID, 0, 114);
    mk_lbl(ram_card, &lv_font_montserrat_14, COL_ACCENT, "RAM");
    s_ram_bar = lv_bar_create(ram_card);
    lv_obj_set_size(s_ram_bar, 132, 12);
    lv_obj_align(s_ram_bar, LV_ALIGN_LEFT_MID, 40, 0);
    lv_bar_set_range(s_ram_bar, 0, 100);
    lv_bar_set_value(s_ram_bar, s_ram, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ram_bar, lv_color_make(15, 20, 38), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ram_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ram_bar, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ram_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ram_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ram_bar, 6, LV_PART_INDICATOR);
    lv_snprintf(buf, sizeof(buf), "%lu%%", (unsigned long)s_ram);
    s_ram_pct_label = mk_lbl(ram_card, &lv_font_montserrat_16, COL_TEXT, buf);
    lv_obj_align(s_ram_pct_label, LV_ALIGN_RIGHT_MID, -6, 0);

    // ---- Bottom: UP + DOWN ----
    lv_obj_t *upl_card = mk_card(p, 112, 60, COL_GREEN, LV_OPA_40);
    lv_obj_align(upl_card, LV_ALIGN_BOTTOM_LEFT, 4, -2);
    {
        lv_obj_t *t = mk_lbl(upl_card, &lv_font_montserrat_14, COL_GREEN, "^ UP");
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 6, 2);
        fmt_speed(buf, sizeof(buf), s_up);
        s_up_speed_label = mk_lbl(upl_card, &lv_font_montserrat_16, COL_TEXT, buf);
        lv_obj_align(s_up_speed_label, LV_ALIGN_BOTTOM_LEFT, 6, -4);
    }

    lv_obj_t *dnl_card = mk_card(p, 112, 60, COL_PURPLE, LV_OPA_40);
    lv_obj_align(dnl_card, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
    {
        lv_obj_t *t = mk_lbl(dnl_card, &lv_font_montserrat_14, COL_PURPLE, "v DOWN");
        lv_obj_align(t, LV_ALIGN_TOP_LEFT, 6, 2);
        fmt_speed(buf, sizeof(buf), s_down);
        s_dn_speed_label = mk_lbl(dnl_card, &lv_font_montserrat_16, COL_TEXT, buf);
        lv_obj_align(s_dn_speed_label, LV_ALIGN_BOTTOM_LEFT, 6, -4);
    }
}

// ============================================================================
//  Data refresh tick
// ============================================================================
static void tick_cb(lv_timer_t *t)
{
    (void)t;

    const NetSync_Data *ns = NetSync_Get();
    if (ns && ns->valid) {
        s_cpu        = ns->cpu;
        s_cpu_freq   = ns->freq_mhz;
        s_cpu_temp   = ns->cpu_temp;

        s_ram        = ns->ram_pct;
        s_up         = ns->up_bps;
        s_down       = ns->down_bps;
        s_uptime     = ns->uptime_sec;

        s_has_cpu_temp = ns->has_cpu_temp;
        s_has_uptime   = ns->has_uptime;

        s_have_data = 1;
    } else if (s_have_data) {
        s_uptime += 1;   // keep ticking locally between PC packets
    }

    char buf[32];

    lv_arc_set_value(s_cpu_arc, s_cpu);
    lv_arc_set_angles(s_cpu_arc, 135, 135 + (270 * s_cpu) / 100);
    lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_cpu);
    lv_label_set_text(s_cpu_pct_label, buf);

    if (s_cpu_freq == 0) {
        lv_label_set_text(s_cpu_temp_label, "--");
    } else {
        // s_cpu_freq is GHz * 100 (e.g. 272 -> "2.72")
        lv_snprintf(buf, sizeof(buf), "%lu.%02lu",
                    (unsigned long)(s_cpu_freq / 100),
                    (unsigned long)(s_cpu_freq % 100));
        lv_label_set_text(s_cpu_temp_label, buf);
    }

    // ============================================================
    // UPTIME / CPU TEMP 显示逻辑
    //
    // 只有 uptime
    //     -> 一直显示 uptime
    //
    // 只有 cpu_t
    //     -> 一直显示 CPU TEMP
    //
    // 两者都有
    //     -> 每 3 秒切换一次
    // ============================================================

    if (s_uptime_home_label &&
        s_top_info_title)
    {
        // --------------------------------------------------------
        // 情况 1：两者都有
        // --------------------------------------------------------

        if (s_has_uptime &&
            s_has_cpu_temp)
        {
            s_switch_seconds++;

            if (s_switch_seconds >= 3)
            {
                s_switch_seconds = 0;
                s_show_cpu_temp =
                    !s_show_cpu_temp;
            }

            if (s_show_cpu_temp)
            {
                // CPU TEMP

                lv_label_set_text(
                    s_top_info_title,
                    "CPU TEMP"
                );

                lv_snprintf(
                    buf,
                    sizeof(buf),
                    "%u°C",
                    (unsigned int)s_cpu_temp
                );

                lv_label_set_text(
                    s_uptime_home_label,
                    buf
                );
            }
            else
            {
                // UPTIME

                lv_label_set_text(
                    s_top_info_title,
                    "UPTIME"
                );

                if (s_uptime == 0)
                {
                    lv_label_set_text(
                        s_uptime_home_label,
                        "--"
                    );
                }
                else
                {
                    fmt_uptime_days(
                        buf,
                        sizeof(buf),
                        s_uptime
                    );

                    lv_label_set_text(
                        s_uptime_home_label,
                        buf
                    );
                }
            }
        }

        // --------------------------------------------------------
        // 情况 2：只有 CPU TEMP
        // --------------------------------------------------------

        else if (s_has_cpu_temp)
        {
            s_switch_seconds = 0;
            s_show_cpu_temp = true;

            lv_label_set_text(
                s_top_info_title,
                "CPU TEMP"
            );

            lv_snprintf(
                buf,
                sizeof(buf),
                "%u°C",
                (unsigned int)s_cpu_temp
            );

            lv_label_set_text(
                s_uptime_home_label,
                buf
            );
        }

        // --------------------------------------------------------
        // 情况 3：只有 UPTIME
        // --------------------------------------------------------

        else if (s_has_uptime)
        {
            s_switch_seconds = 0;
            s_show_cpu_temp = false;

            lv_label_set_text(
                s_top_info_title,
                "UPTIME"
            );

            if (s_uptime == 0)
            {
                lv_label_set_text(
                    s_uptime_home_label,
                    "--"
                );
            }
            else
            {
                fmt_uptime_days(
                    buf,
                    sizeof(buf),
                    s_uptime
                );

                lv_label_set_text(
                    s_uptime_home_label,
                    buf
                );
            }
        }

        // --------------------------------------------------------
        // 情况 4：两个都没有
        // --------------------------------------------------------

        else
        {
            s_switch_seconds = 0;
            s_show_cpu_temp = false;

            lv_label_set_text(
                s_top_info_title,
                "SYSTEM"
            );

            lv_label_set_text(
                s_uptime_home_label,
                "--"
            );
        }
    }

    lv_bar_set_value(s_ram_bar, s_ram, LV_ANIM_OFF);
    lv_snprintf(buf, sizeof(buf), "%lu%%", (unsigned long)s_ram);
    lv_label_set_text(s_ram_pct_label, buf);

    fmt_speed(buf, sizeof(buf), s_up);
    if (s_up_speed_label) lv_label_set_text(s_up_speed_label, buf);
    fmt_speed(buf, sizeof(buf), s_down);
    if (s_dn_speed_label) lv_label_set_text(s_dn_speed_label, buf);
}

// ============================================================================
//  Public entry
// ============================================================================
void Dashboard_Start(void)
{
    s_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_root, 240, 240);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    build_chrome();
    build_page_system();

    lv_timer_create(tick_cb, 1000, NULL);
}
