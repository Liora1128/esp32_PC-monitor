// Flashlight app: turns the backlight up to 100% and shows a white
// screen. The OK key exits and restores the previous brightness.

#include "App_Flashlight.h"
#include "Menu_System.h"

#include <lvgl.h>

static float s_prev_brightness = 1.0f;
static lv_obj_t *s_white = NULL;

static void flashlight_enter(void)
{
    // Push the backlight all the way up. (We don't actually know the
    // previous value, so just remember 1.0 as a sane restore target.)
    s_prev_brightness = 1.0f;
    TFT_BL_SetBrightness(1.0f);

    lv_obj_t *view = MenuSystem_GetAppView();
    s_white = lv_obj_create(view);
    lv_obj_set_size(s_white, 240, 240);
    lv_obj_set_pos(s_white, 0, 0);
    lv_obj_set_style_bg_color(s_white, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_white, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_white, 0, 0);

    MenuSystem_ShowAppTitle(false);
}

static void flashlight_exit(void)
{
    if (s_white) {
        lv_obj_del(s_white);
        s_white = NULL;
    }
    TFT_BL_SetBrightness(s_prev_brightness);
}

const menu_app_t App_Flashlight = {
    .name  = "Flashlight",
    .color = LV_COLOR_MAKE(255, 220, 90),
    .enter = flashlight_enter,
    .exit  = flashlight_exit,
};