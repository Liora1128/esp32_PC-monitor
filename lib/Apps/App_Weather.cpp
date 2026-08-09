// Weather app: shows the latest text that the Weather module produced.
// The Weather_StartPeriodic() task already fetches every 30 minutes, so
// we just read whatever was last reported. LEFT/RIGHT trigger an
// immediate refresh (blocking, ~8 s).

#include "App_Weather.h"
#include "Menu_System.h"
#include "Weather.h"

#include <lvgl.h>

static lv_obj_t *s_temp_label  = NULL;
static lv_obj_t *s_status_label = NULL;

// Public hook Weather uses to push data into this app.
static void weather_app_cb(const char *text, void *user)
{
    (void)user;
    if (s_temp_label) lv_label_set_text(s_temp_label, text);
    if (s_status_label) lv_label_set_text(s_status_label, "Updated");
}

static void weather_enter(void)
{
    // Register our callback so Weather refreshes push to this label.
    Weather_Init(weather_app_cb, NULL);

    lv_obj_t *body = MenuSystem_GetAppBody();
    MenuSystem_ShowAppTitle(true);

    s_temp_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_temp_label, lv_color_make(255, 220, 120), 0);
    lv_obj_align(s_temp_label, LV_ALIGN_CENTER, 0, -10);

    s_status_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_status_label, lv_color_make(140, 160, 180), 0);
    lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Pull an immediate value via the shared callback chain
    Weather_FetchOnce();
}

static void weather_exit(void)
{
    s_temp_label = NULL;
    s_status_label = NULL;
}

static bool weather_on_key(menu_key_t key)
{
    if (key == MENU_KEY_OK) {
        if (s_status_label) lv_label_set_text(s_status_label, "Refreshing...");
        Weather_FetchOnce();
        return true;  // keep weather open
    }
    return false;
}

const menu_app_t App_Weather = {
    .name   = "Weather",
    .color  = LV_COLOR_MAKE(90, 200, 255),
    .enter  = weather_enter,
    .exit   = weather_exit,
    .on_key = weather_on_key,
};