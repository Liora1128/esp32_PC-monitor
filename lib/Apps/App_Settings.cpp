// Settings app: a small read-only summary of system info.
// LEFT/RIGHT are not used; OK exits back to the carousel.

#include "App_Settings.h"
#include "Menu_System.h"

#include <lvgl.h>

static lv_obj_t *s_info_label = NULL;

static void settings_enter(void)
{
    lv_obj_t *body = MenuSystem_GetAppBody();
    MenuSystem_ShowAppTitle(true);

    s_info_label = lv_label_create(body);
    lv_label_set_text(s_info_label,
        "WiFi: Xiaomi_A520\n"
        "TZ:   CST-8\n"
        "Bright: 100%\n"
        "Version: 1.0");
    lv_obj_set_style_text_font(s_info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_info_label, lv_color_make(220, 200, 200), 0);
    lv_obj_set_style_text_letter_space(s_info_label, 1, 0);
    lv_obj_align(s_info_label, LV_ALIGN_TOP_LEFT, 4, 4);

    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, "OK = back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_make(140, 160, 180), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
}

static void settings_exit(void)
{
    s_info_label = NULL;
}

const menu_app_t App_Settings = {
    .name  = "Settings",
    .color = LV_COLOR_MAKE(220, 100, 100),
    .enter = settings_enter,
    .exit  = settings_exit,
};