// Tiny reflex game: press LEFT/RIGHT alternately to fill a progress bar.
// This is a stub - real game logic can be added later.

#include "App_Games.h"
#include "Menu_System.h"

#include <lvgl.h>

static int      s_score = 0;
static lv_obj_t *s_score_label = NULL;
static lv_obj_t *s_bar        = NULL;

static void refresh(void)
{
    if (s_score_label) {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "Score: %d", s_score);
        lv_label_set_text(s_score_label, buf);
    }
    if (s_bar) {
        lv_bar_set_value(s_bar, (s_score * 10) % 101, LV_ANIM_OFF);
    }
}

static void games_enter(void)
{
    s_score = 0;
    lv_obj_t *body = MenuSystem_GetAppBody();
    MenuSystem_ShowAppTitle(true);

    s_score_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_score_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_score_label, lv_color_white(), 0);
    lv_obj_align(s_score_label, LV_ALIGN_TOP_MID, 0, 8);

    s_bar = lv_bar_create(body);
    lv_obj_set_size(s_bar, 180, 12);
    lv_obj_align(s_bar, LV_ALIGN_CENTER, 0, 4);
    lv_bar_set_range(s_bar, 0, 100);

    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, "Press OK to score!");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_make(140, 160, 180), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    refresh();
}

static void games_exit(void)
{
    s_score_label = NULL;
    s_bar         = NULL;
    s_score       = 0;
}

static bool games_on_key(menu_key_t key)
{
    if (key == MENU_KEY_OK) {
        s_score += 1;
        refresh();
        return true;  // OK stays in game (don't exit)
    }
    if (key == MENU_KEY_LEFT || key == MENU_KEY_RIGHT) {
        // LEFT/RIGHT reduce the score for variety.
        if (s_score > 0) s_score -= 1;
        refresh();
    }
    return false;
}

const menu_app_t App_Games = {
    .name   = "Games",
    .color  = LV_COLOR_MAKE(140, 90, 255),
    .enter  = games_enter,
    .exit   = games_exit,
    .on_key = games_on_key,
};