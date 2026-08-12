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

// 新版 My_Button 是中断 + esp_timer 实现，事件已经合成好推到
// FreeRTOS 队列里。这里只需要阻塞读事件，并把 SHORT 转发给
// 菜单系统（其它事件目前先不消费，留给后续扩展）。
//
// pin -> menu key 的映射：
//   GPIO 1  -> MENU_KEY_LEFT
//   GPIO 2  -> MENU_KEY_OK
//   GPIO 42 -> MENU_KEY_RIGHT
//
// menu_key_t 没有 "NONE" 枚举，所以用一个 bool + 指针型的输出参数
// 来表达"这个 pin 不是菜单按钮"的情况。
static bool pin_to_menu_key(gpio_num_t pin, menu_key_t *out)
{
    menu_key_t k;
    switch (pin) {
        case GPIO_NUM_1:  k = MENU_KEY_LEFT;  break;
        case GPIO_NUM_2:  k = MENU_KEY_OK;    break;
        case GPIO_NUM_42: k = MENU_KEY_RIGHT; break;
        default:          return false;
    }
    if (out) *out = k;
    return true;
}

static void button_event_task(void *arg)
{
    (void)arg;

    KeyEvent ev;
    while (1) {
        // 永久阻塞，直到 ISR/timer 推事件过来。
        KeyState st = My_Button_GetEvent(&ev, portMAX_DELAY);
        if (st == KEY_NONE) continue;

        if (st == KEY_SHORT) {
            menu_key_t k;
            if (pin_to_menu_key(ev.pin, &k)) {
                // MenuSystem_HandleKey 内部会用 lv_async_call
                // 把调用转到 LVGL 线程，所以从这任务直接调安全。
                MenuSystem_HandleKey(k);
            }
        }
        // KEY_LONG / KEY_DOUBLE / KEY_PRESSED 目前先不消费，
        // 等具体 UI 场景需要时再在这里加分支。
    }
}

void BtnPoll_StartBackground(void)
{
    static int started = 0;
    if (started) return;
    started = 1;
    // 注意：栈从 4096 缩到 2048 也够，这里只是消费者任务，
    // 调用链很短。保留 4096 让编译器和未来扩展都更从容。
    xTaskCreate(button_event_task, "btn_evt", 4096, NULL, 2, NULL);
}