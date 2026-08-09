#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "lvgl.h"

// The "page" the screen is currently showing.
typedef enum {
    MENU_PAGE_CLOCK = 0,     // home / watch face
    MENU_PAGE_APP_MENU,      // app grid (carousel)
    MENU_PAGE_APP_RUNNING,   // a specific app's UI
} menu_page_t;

// Physical button indices reported by the button layer.
// (Matches the wiring: GPIO 1 = left, GPIO 42 = right, GPIO 2 = ok.)
typedef enum {
    MENU_KEY_LEFT = 0,
    MENU_KEY_OK   = 1,
    MENU_KEY_RIGHT = 2,
} menu_key_t;

// One app entry. Apps register themselves via MenuSystem_RegisterApp().
typedef struct {
    const char *name;        // shown on the carousel
    lv_color_t  color;       // accent colour for the icon background
    void (*enter)(void);     // called when the user presses OK to open
    void (*exit)(void);      // called when the user leaves the app
    // Optional - called for LEFT/RIGHT/OK while the app is open.
    // Returns true if it consumed the OK key (in which case the menu
    // layer will NOT auto-exit); the menu layer always handles LEFT
    // and RIGHT itself.
    bool (*on_key)(menu_key_t);
} menu_app_t;

// Build the carousel + overlay. Call once from app_main after LVGL init.
void MenuSystem_Build(void);

// Register an app. Call before MenuSystem_Build (or anytime - the
// carousel is rebuilt on the next open).
void MenuSystem_RegisterApp(const menu_app_t *app);

// Dispatch a SHORT key press. Safe from any task; marshals to the
// LVGL thread internally via lv_async_call.
void MenuSystem_HandleKey(menu_key_t key);

// Helpers for apps that want to draw inside the generic body panel.
lv_obj_t *MenuSystem_GetAppBody(void);
lv_obj_t *MenuSystem_GetAppView(void);
void      MenuSystem_ShowAppTitle(bool show);

#endif