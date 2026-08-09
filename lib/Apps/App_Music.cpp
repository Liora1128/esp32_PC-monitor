// Music app: a placeholder "now playing" card.
// Real audio playback would require I2S + decoder; for now this just
// shows song info + a play/pause hint that toggles with LEFT/RIGHT.

#include "App_Music.h"
#include "Menu_System.h"

#include <lvgl.h>

static bool s_playing = false;
static int  s_track   = 1;
static lv_obj_t *s_state_label  = NULL;
static lv_obj_t *s_track_label  = NULL;

static void refresh(void)
{
    if (s_state_label) {
        lv_label_set_text(s_state_label, s_playing ? "Playing" : "Paused");
    }
    if (s_track_label) {
        char buf[24];
        lv_snprintf(buf, sizeof(buf), "Track %d / 5", s_track);
        lv_label_set_text(s_track_label, buf);
    }
}

static void music_enter(void)
{
    lv_obj_t *body = MenuSystem_GetAppBody();
    MenuSystem_ShowAppTitle(true);

    s_track_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_track_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_track_label, lv_color_white(), 0);
    lv_obj_align(s_track_label, LV_ALIGN_TOP_MID, 0, 8);

    s_state_label = lv_label_create(body);
    lv_obj_set_style_text_font(s_state_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_state_label, lv_color_make(0, 220, 255), 0);
    lv_obj_align(s_state_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, "<- prev   play/pause   next ->");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_make(140, 160, 180), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    refresh();
}

static void music_exit(void)
{
    s_state_label = NULL;
    s_track_label = NULL;
    s_playing = false;
}

static bool music_key(menu_key_t key)
{
    if (key == MENU_KEY_LEFT) {
        s_track = (s_track <= 1) ? 5 : s_track - 1;
        refresh();
    } else if (key == MENU_KEY_RIGHT) {
        s_track = (s_track >= 5) ? 1 : s_track + 1;
        refresh();
    } else if (key == MENU_KEY_OK) {
        s_playing = !s_playing;
        refresh();
    }
    return false;  // let OK exit the music app (long-press for power, etc.)
}

const menu_app_t App_Music = {
    .name   = "Music",
    .color  = LV_COLOR_MAKE(255, 90, 90),
    .enter  = music_enter,
    .exit   = music_exit,
    .on_key = music_key,
};