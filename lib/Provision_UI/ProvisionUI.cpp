#include "ProvisionUI.h"

#include "lvgl.h"
#include <string.h>

#define UI_BG       LV_COLOR_MAKE(8, 12, 24)
#define UI_WHITE    LV_COLOR_MAKE(240, 245, 255)
#define UI_DIM      LV_COLOR_MAKE(140, 150, 175)
#define UI_ACCENT   LV_COLOR_MAKE(0, 229, 255)
#define UI_GREEN    LV_COLOR_MAKE(16, 200, 130)

static lv_obj_t *s_root = nullptr;

static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_status = nullptr;
static lv_obj_t *s_wifi_name = nullptr;

static lv_obj_t *s_arc = nullptr;

static lv_obj_t *s_dot1 = nullptr;
static lv_obj_t *s_dot2 = nullptr;
static lv_obj_t *s_dot3 = nullptr;

static bool s_active = false;

static int s_angle = 0;
static int s_dot_phase = 0;

// ============================================================
// Label
// ============================================================

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const lv_font_t *font,
    lv_color_t color,
    const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(
        label,
        text
    );

    lv_obj_set_style_text_font(
        label,
        font,
        0
    );

    lv_obj_set_style_text_color(
        label,
        color,
        0
    );

    return label;
}

// ============================================================
// 动画小点
// ============================================================

static lv_obj_t *create_dot(
    lv_obj_t *parent)
{
    lv_obj_t *dot = lv_obj_create(parent);

    lv_obj_set_size(
        dot,
        6,
        6
    );

    lv_obj_set_style_radius(
        dot,
        LV_RADIUS_CIRCLE,
        0
    );

    lv_obj_set_style_bg_color(
        dot,
        UI_ACCENT,
        0
    );

    lv_obj_set_style_bg_opa(
        dot,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        dot,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        dot,
        0,
        0
    );

    return dot;
}

// ============================================================
// 开始显示配网页面
// ============================================================

void ProvisionUI_Start()
{
    s_active = true;

    s_angle = 0;
    s_dot_phase = 0;

    lv_obj_clean(
        lv_scr_act()
    );

    lv_obj_set_style_bg_color(
        lv_scr_act(),
        UI_BG,
        0
    );

    lv_obj_set_style_bg_opa(
        lv_scr_act(),
        LV_OPA_COVER,
        0
    );

    // --------------------------------------------------------
    // Root
    // --------------------------------------------------------

    s_root = lv_obj_create(
        lv_scr_act()
    );

    lv_obj_set_size(
        s_root,
        240,
        240
    );

    lv_obj_set_pos(
        s_root,
        0,
        0
    );

    lv_obj_set_style_bg_opa(
        s_root,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        s_root,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        s_root,
        0,
        0
    );

    // --------------------------------------------------------
    // PCMonitor
    // --------------------------------------------------------

    lv_obj_t *brand = create_label(
        s_root,
        &lv_font_montserrat_24,
        UI_WHITE,
        "PCMonitor"
    );

    lv_obj_align(
        brand,
        LV_ALIGN_TOP_MID,
        0,
        14
    );

    // --------------------------------------------------------
    // Wi-Fi Setup
    // --------------------------------------------------------

    s_title = create_label(
        s_root,
        &lv_font_montserrat_16,
        UI_ACCENT,
        "Wi-Fi Setup"
    );

    lv_obj_align(
        s_title,
        LV_ALIGN_TOP_MID,
        0,
        48
    );

    // --------------------------------------------------------
    // 单圆环
    // --------------------------------------------------------

    s_arc = lv_arc_create(
        s_root
    );

    lv_obj_set_size(
        s_arc,
        90,
        90
    );

    lv_obj_align(
        s_arc,
        LV_ALIGN_CENTER,
        0,
        -10
    );

    lv_arc_set_range(
        s_arc,
        0,
        360
    );

    lv_arc_set_bg_angles(
        s_arc,
        0,
        360
    );

    lv_arc_set_value(
        s_arc,
        270
    );

    lv_obj_set_style_arc_color(
        s_arc,
        lv_color_make(25, 35, 60),
        LV_PART_MAIN
    );

    lv_obj_set_style_arc_width(
        s_arc,
        9,
        LV_PART_MAIN
    );

    lv_obj_set_style_arc_color(
        s_arc,
        UI_ACCENT,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_width(
        s_arc,
        9,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_arc_rounded(
        s_arc,
        true,
        LV_PART_INDICATOR
    );

    lv_obj_remove_style(
        s_arc,
        NULL,
        LV_PART_KNOB
    );

    // --------------------------------------------------------
    // 状态
    // --------------------------------------------------------

    s_status = create_label(
        s_root,
        &lv_font_montserrat_14,
        UI_DIM,
        "Starting..."
    );

    lv_obj_set_width(
        s_status,
        220
    );

    lv_label_set_long_mode(
        s_status,
        LV_LABEL_LONG_WRAP
    );

    lv_obj_set_style_text_align(
        s_status,
        LV_TEXT_ALIGN_CENTER,
        0
    );

    lv_obj_align(
        s_status,
        LV_ALIGN_CENTER,
        0,
        50
    );

    // --------------------------------------------------------
    // Wi-Fi 名称
    // --------------------------------------------------------

    s_wifi_name = create_label(
        s_root,
        &lv_font_montserrat_16,
        UI_WHITE,
        "PCMonitor-Setup"
    );

    lv_obj_set_width(
        s_wifi_name,
        220
    );

    lv_label_set_long_mode(
        s_wifi_name,
        LV_LABEL_LONG_DOT
    );

    lv_obj_set_style_text_align(
        s_wifi_name,
        LV_TEXT_ALIGN_CENTER,
        0
    );

    lv_obj_align(
        s_wifi_name,
        LV_ALIGN_CENTER,
        0,
        75
    );

    // --------------------------------------------------------
    // 三个点
    // --------------------------------------------------------

    s_dot1 = create_dot(s_root);
    s_dot2 = create_dot(s_root);
    s_dot3 = create_dot(s_root);

    lv_obj_align(
        s_dot1,
        LV_ALIGN_BOTTOM_MID,
        -12,
        -18
    );

    lv_obj_align(
        s_dot2,
        LV_ALIGN_BOTTOM_MID,
        0,
        -18
    );

    lv_obj_align(
        s_dot3,
        LV_ALIGN_BOTTOM_MID,
        12,
        -18
    );
}

// ============================================================
// 设置 Wi-Fi 名称
// ============================================================

void ProvisionUI_SetWifiName(
    const char *ssid)
{
    if (!s_active || !s_wifi_name)
        return;

    if (!ssid || ssid[0] == '\0')
    {
        lv_label_set_text(
            s_wifi_name,
            ""
        );

        return;
    }

    lv_label_set_text(
        s_wifi_name,
        ssid
    );
}

// ============================================================
// 设置“等待手机配网”
// ============================================================

void ProvisionUI_ShowWaiting()
{
    if (!s_active)
        return;

    lv_obj_set_style_arc_color(
        s_arc,
        UI_ACCENT,
        LV_PART_INDICATOR
    );

    lv_label_set_text(
        s_title,
        "Wi-Fi Setup"
    );

    lv_obj_set_style_text_color(
        s_title,
        UI_ACCENT,
        0
    );

    lv_label_set_text(
        s_status,
        "Connect your phone to:"
    );

    lv_label_set_text(
        s_wifi_name,
        "PCMonitor-Setup"
    );
}

// ============================================================
// 设置“连接中”
// ============================================================

void ProvisionUI_ShowConnecting()
{
    if (!s_active)
        return;

    lv_obj_set_style_arc_color(
        s_arc,
        UI_ACCENT,
        LV_PART_INDICATOR
    );

    lv_label_set_text(
        s_title,
        "Wi-Fi Setup"
    );

    lv_obj_set_style_text_color(
        s_title,
        UI_ACCENT,
        0
    );

    lv_label_set_text(
        s_status,
        "Connecting..."
    );
}

// ============================================================
// 设置“连接成功，准备重启”
// ============================================================

void ProvisionUI_ShowSuccess()
{
    if (!s_active)
        return;

    lv_obj_set_style_arc_color(
        s_arc,
        UI_GREEN,
        LV_PART_INDICATOR
    );

    lv_label_set_text(
        s_title,
        "Wi-Fi Connected"
    );

    lv_obj_set_style_text_color(
        s_title,
        UI_GREEN,
        0
    );

    lv_label_set_text(
        s_status,
        "Setup complete"
    );
}

// ============================================================
// 持续动画
// ============================================================

void ProvisionUI_Update()
{
    if (!s_active)
        return;

    // --------------------------------------------------------
    // 单圆环持续旋转
    // --------------------------------------------------------

    s_angle += 7;

    if (s_angle >= 360)
        s_angle -= 360;

    lv_arc_set_value(
        s_arc,
        s_angle
    );

    // --------------------------------------------------------
    // 三个点
    // --------------------------------------------------------

    s_dot_phase++;

    if (s_dot_phase >= 30)
        s_dot_phase = 0;

    int a1 = 100;
    int a2 = 100;
    int a3 = 100;

    if (s_dot_phase < 10)
    {
        a1 = 255;
    }
    else if (s_dot_phase < 20)
    {
        a2 = 255;
    }
    else
    {
        a3 = 255;
    }

    lv_obj_set_style_opa(
        s_dot1,
        a1,
        0
    );

    lv_obj_set_style_opa(
        s_dot2,
        a2,
        0
    );

    lv_obj_set_style_opa(
        s_dot3,
        a3,
        0
    );
}

// ============================================================
// 查询是否正在显示
// ============================================================

bool ProvisionUI_IsActive()
{
    return s_active;
}