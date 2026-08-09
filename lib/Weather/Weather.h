#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>
#include <stdint.h>

// Blocking HTTP fetch from wttr.in. Returns true on success.
// On success, calls the registered callback with a short string
// like " +20C" (icon + temperature, no city).
typedef void (*weather_cb_t)(const char *text, void *user);

void Weather_Init(weather_cb_t cb, void *user);

// Fetch weather once, synchronously. Blocks up to 8 seconds.
// Designed to be called from a background task.
bool Weather_FetchOnce(void);

// Start a FreeRTOS task that re-fetches weather every `period_ms`
// milliseconds. Safe to call once.
void Weather_StartPeriodic(uint32_t period_ms);

#endif