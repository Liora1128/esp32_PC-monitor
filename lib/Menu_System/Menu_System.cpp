// Smartwatch-style app carousel.
// The home screen (clock face) shows two small corner buttons; pressing
// the LEFT button opens the carousel. In the carousel, LEFT/RIGHT
// rotate through registered apps and OK enters the highlighted one.
// Inside an app, OK returns to the carousel and LEFT/RIGHT are passed
// through to the app for its own use.

#include "Menu_System.h"

#include <string.h>

// ----------------------------------------------------------------------
//  App registry
// ----------------------------------------------------------------------

#define MAX_APPS 8

static const menu_app_t *s_apps[MAX_APPS];
static int s_app_count = 0;

// Currently running app (NULL when on home or carousel).
static int s_running_app = -1;

// State machine state.
static menu_page_t s_page = MENU_PAGE_CLOCK;
static int         s_carousel_index = 0;   // highlighted app in carousel

// ----------------------------------------------------------------------
//  LV objects (created lazily on first Build)
// ----------------------------------------------------------------------

// Home-screen corner buttons
static lv_obj_t *s_home_menu_btn;     // bottom-left, opens the carousel
static lv_obj_t *s_home_clock_label;  // not used; kept for clarity

// Carousel widgets
static lv_obj_t *s_overlay;
static lv_obj_t *s_title;
static lv_obj_t *s_card_left;         // dim preview of prev app
static lv_obj_t *s_card_center;       // highlighted current app
static lv_obj_t *s_card_right;        // dim preview of next app
static lv_obj_t *s_card_left_label;
static lv_obj_t *s_card_center_label;
static lv_obj_t *s_card_right_label;
static lv_obj_t *s_dot_row;
static lv_obj_t *s_hint_label;        // bottom hint "OK = open"

// Generic "app view" overlay - shown when an app is running
static lv_obj_t *s_app_view;
static lv_obj_t *s_app_title;
static lv_obj_t *s_app_body;          // apps fill this with their own UI

// ----------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------

static lv_obj_t *make_text(lv_obj_t *parent, lv_color_t color,
                           const lv_font_t *font, const char *text,
                           lv_align_t align, int x_ofs, int y_ofs)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    lv_obj_align(l, align, x_ofs, y_ofs);
    return l;
}

static void show_overlay(void)
{
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_dot_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_card_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_card_center, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_card_right, LV_OBJ_FLAG_HIDDEN);
}

static void hide_overlay(void)
{
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_dot_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_card_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_card_center, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_card_right, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_carousel(void)
{
    if (s_app_count == 0) return;

    int n = s_app_count;
    int idx = s_carousel_index;
    int prev = (idx - 1 + n) % n;
    int next = (idx + 1) % n;

    const menu_app_t *p = s_apps[prev];
    const menu_app_t *c = s_apps[idx];
    const menu_app_t *q = s_apps[next];

    lv_label_set_text(s_card_left_label,   p->name);
    lv_label_set_text(s_card_center_label, c->name);
    lv_label_set_text(s_card_right_label,  q->name);

    lv_obj_set_style_bg_color(s_card_left,   p->color, 0);
    lv_obj_set_style_bg_color(s_card_center, c->color, 0);
    lv_obj_set_style_bg_color(s_card_right,  q->color, 0);

    // Dots: "● ○ ○" style indicator
    char dots[24] = "";
    for (int i = 0; i < n; i++) {
        if ((int)strlen(dots) + 4 >= (int)sizeof(dots)) break;
        if (i == idx) strcat(dots, "● ");
        else          strcat(dots, "○ ");
    }
    lv_label_set_text(s_dot_row, dots);
}

static void open_carousel(void)
{
    s_page = MENU_PAGE_APP_MENU;
    s_carousel_index = 0;
    refresh_carousel();
    show_overlay();
}

static void close_carousel(void)
{
    s_page = MENU_PAGE_CLOCK;
    hide_overlay();
}

static void enter_app(int idx)
{
    if (idx < 0 || idx >= s_app_count) return;
    s_running_app = idx;
    s_page = MENU_PAGE_APP_RUNNING;

    // Hide carousel, show generic app view
    hide_overlay();
    lv_label_set_text(s_app_title, s_apps[idx]->name);
    lv_obj_clear_flag(s_app_view, LV_OBJ_FLAG_HIDDEN);

    if (s_apps[idx]->enter) s_apps[idx]->enter();
}

static void exit_app(void)
{
    if (s_running_app >= 0) {
        if (s_apps[s_running_app]->exit) s_apps[s_running_app]->exit();
        s_running_app = -1;
    }
    lv_obj_add_flag(s_app_view, LV_OBJ_FLAG_HIDDEN);
    open_carousel();
}

// ----------------------------------------------------------------------
//  Public API
// ----------------------------------------------------------------------

void MenuSystem_RegisterApp(const menu_app_t *app)
{
    if (!app || s_app_count >= MAX_APPS) return;
    s_apps[s_app_count++] = app;
}

void MenuSystem_Build(void)
{
    // ----- Home-screen left button (opens the carousel) -----
    s_home_menu_btn = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_home_menu_btn, 64, 28);
    lv_obj_set_style_radius(s_home_menu_btn, 14, 0);
    lv_obj_set_style_bg_color(s_home_menu_btn, lv_color_make(20, 30, 60), 0);
    lv_obj_set_style_border_color(s_home_menu_btn, lv_color_make(0, 180, 255), 0);
    lv_obj_set_style_border_width(s_home_menu_btn, 1, 0);
    lv_obj_align(s_home_menu_btn, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_t *mt = lv_label_create(s_home_menu_btn);
    lv_label_set_text(mt, "Menu");
    lv_obj_set_style_text_font(mt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mt, lv_color_make(0, 220, 255), 0);
    lv_obj_center(mt);

    // ----- Dim background overlay -----
    s_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_overlay, 240, 240);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    s_title = make_text(lv_scr_act(), lv_color_make(200, 220, 240),
                        &lv_font_montserrat_16, "Apps",
                        LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_add_flag(s_title, LV_OBJ_FLAG_HIDDEN);

    // ----- Three cards in a row: prev | current | next -----
    s_card_left = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_card_left, 60, 90);
    lv_obj_align(s_card_left, LV_ALIGN_LEFT_MID, 6, -4);
    lv_obj_set_style_radius(s_card_left, 10, 0);
    lv_obj_set_style_bg_opa(s_card_left, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_card_left, 0, 0);
    lv_obj_add_flag(s_card_left, LV_OBJ_FLAG_HIDDEN);
    s_card_left_label = lv_label_create(s_card_left);
    lv_label_set_text(s_card_left_label, "");
    lv_obj_set_style_text_font(s_card_left_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_card_left_label, lv_color_make(220, 220, 230), 0);
    lv_obj_center(s_card_left_label);

    s_card_center = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_card_center, 100, 120);
    lv_obj_align(s_card_center, LV_ALIGN_CENTER, 0, -4);
    lv_obj_set_style_radius(s_card_center, 14, 0);
    lv_obj_set_style_bg_opa(s_card_center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_card_center, 3, 0);
    lv_obj_set_style_border_color(s_card_center, lv_color_white(), 0);
    lv_obj_add_flag(s_card_center, LV_OBJ_FLAG_HIDDEN);
    s_card_center_label = lv_label_create(s_card_center);
    lv_label_set_text(s_card_center_label, "");
    lv_obj_set_style_text_font(s_card_center_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_card_center_label, lv_color_white(), 0);
    lv_obj_center(s_card_center_label);

    s_card_right = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_card_right, 60, 90);
    lv_obj_align(s_card_right, LV_ALIGN_RIGHT_MID, -6, -4);
    lv_obj_set_style_radius(s_card_right, 10, 0);
    lv_obj_set_style_bg_opa(s_card_right, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_card_right, 0, 0);
    lv_obj_add_flag(s_card_right, LV_OBJ_FLAG_HIDDEN);
    s_card_right_label = lv_label_create(s_card_right);
    lv_label_set_text(s_card_right_label, "");
    lv_obj_set_style_text_font(s_card_right_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_card_right_label, lv_color_make(220, 220, 230), 0);
    lv_obj_center(s_card_right_label);

    // Position dots + hint at the bottom
    s_dot_row = make_text(lv_scr_act(), lv_color_make(180, 200, 220),
                          &lv_font_montserrat_14, "",
                          LV_ALIGN_BOTTOM_MID, 0, -36);
    lv_obj_add_flag(s_dot_row, LV_OBJ_FLAG_HIDDEN);

    s_hint_label = make_text(lv_scr_act(), lv_color_make(140, 160, 180),
                             &lv_font_montserrat_12,
                             "<- prev    OK open    next ->",
                             LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(s_hint_label, LV_OBJ_FLAG_HIDDEN);

    // ----- Generic app view (filled by apps on enter) -----
    s_app_view = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_app_view, 240, 240);
    lv_obj_set_pos(s_app_view, 0, 0);
    lv_obj_set_style_bg_color(s_app_view, lv_color_make(5, 5, 15), 0);
    lv_obj_set_style_bg_opa(s_app_view, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_app_view, 0, 0);
    lv_obj_add_flag(s_app_view, LV_OBJ_FLAG_HIDDEN);

    s_app_title = make_text(lv_scr_act(), lv_color_white(),
                            &lv_font_montserrat_16, "",
                            LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_add_flag(s_app_title, LV_OBJ_FLAG_HIDDEN);

    s_app_body = lv_obj_create(s_app_view);
    lv_obj_set_size(s_app_body, 220, 160);
    lv_obj_align(s_app_body, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_color(s_app_body, lv_color_make(15, 20, 35), 0);
    lv_obj_set_style_border_color(s_app_body, lv_color_make(60, 80, 110), 0);
    lv_obj_set_style_border_width(s_app_body, 1, 0);
    lv_obj_set_style_radius(s_app_body, 12, 0);
    lv_obj_set_style_pad_all(s_app_body, 8, 0);
    lv_obj_add_flag(s_app_body, LV_OBJ_FLAG_HIDDEN);
}

// ----------------------------------------------------------------------
//  Key dispatch (runs on LVGL thread via lv_async_call)
// ----------------------------------------------------------------------

static void dispatch_key(void *p)
{
    menu_key_t key = (menu_key_t)(intptr_t)p;

    switch (s_page) {
    case MENU_PAGE_CLOCK:
        // Home: LEFT opens the carousel. RIGHT/OK reserved for future.
        if (key == MENU_KEY_LEFT) open_carousel();
        break;

    case MENU_PAGE_APP_MENU:
        if (key == MENU_KEY_LEFT) {
            if (s_app_count > 0) {
                s_carousel_index = (s_carousel_index - 1 + s_app_count)
                                   % s_app_count;
                refresh_carousel();
            }
        } else if (key == MENU_KEY_RIGHT) {
            if (s_app_count > 0) {
                s_carousel_index = (s_carousel_index + 1) % s_app_count;
                refresh_carousel();
            }
        } else if (key == MENU_KEY_OK) {
            enter_app(s_carousel_index);
        }
        break;

    case MENU_PAGE_APP_RUNNING: {
        bool consumed_ok = false;
        if (s_running_app >= 0 && s_apps[s_running_app]->on_key) {
            consumed_ok = s_apps[s_running_app]->on_key(key);
        }
        if (key == MENU_KEY_OK && !consumed_ok) {
            exit_app();
        }
        break;
    }
    }
}

void MenuSystem_HandleKey(menu_key_t key)
{
    lv_async_call(dispatch_key, (void *)(intptr_t)key);
}

// App authors can grab these if they want to render their own UI inside
// the generic app body panel.
lv_obj_t *MenuSystem_GetAppBody(void)        { return s_app_body; }
lv_obj_t *MenuSystem_GetAppView(void)        { return s_app_view; }
void      MenuSystem_ShowAppTitle(bool show) {
    if (show) lv_obj_clear_flag(s_app_title, LV_OBJ_FLAG_HIDDEN);
    else      lv_obj_add_flag(s_app_title, LV_OBJ_FLAG_HIDDEN);
}