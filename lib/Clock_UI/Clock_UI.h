#ifndef CLOCK_UI_H
#define CLOCK_UI_H

#include "lvgl.h"

// Build the clock face UI: numerals, time, date.
// Must be called AFTER lvgl/driver init and AFTER Clock_UI_Init.
void ClockUI_Build(void);

// Update the on-screen clock + date from the current system time.
void ClockUI_RefreshTime(void);

// Register a 1-second LVGL timer that calls ClockUI_RefreshTime().
void ClockUI_StartAutoRefresh(uint32_t period_ms);

#endif