// All FreeRTOS tasks for the watch live here. main.cpp just calls the
// *Start* helpers below; the task bodies stay in this file so the
// main program stays focused on UI / app flow.
//
// This module is placed under lib/RtosTasks/ (not src/) on purpose:
// ESP-IDF's CMake treats any file named "freertos.*" inside src/ as
// part of the FreeRTOS component itself, which collides with the real
// FreeRTOS sources. Living under lib/ avoids that collision.

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "My_Button.h"
#include "Menu_System.h"
#include "freertos.h"

// 10 ms button polling: scans three GPIOs and forwards SHORT presses
// to the menu system. MenuSystem_HandleKey internally marshals to
// the LVGL thread via lv_async_call, so it's safe to call from here.
static void button_poll_task(void *arg)
{
    (void)arg;
    const gpio_num_t pins[3] = { GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_42 };
    const menu_key_t keys[3] = { MENU_KEY_LEFT, MENU_KEY_OK, MENU_KEY_RIGHT };
    while (1) {
        for (int i = 0; i < 3; i++) {
            if (My_Button_Scan(pins[i]) == KEY_SHORT) {
                MenuSystem_HandleKey(keys[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void BtnPoll_StartBackground(void)
{
    static int started = 0;
    if (started) return;
    started = 1;
    xTaskCreate(button_poll_task, "btn_poll", 4096, NULL, 2, NULL);
}