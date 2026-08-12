#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "My_Button.h"
#include "freertos.h"

// ============================================================
// My_Button 事件后台消费者
// ============================================================
//
// 当前项目的按钮事件主要由 main.cpp 中的 BOOT 5 秒监听任务使用。
// 这里保留一个后台消费者，避免旧版 Menu_System 依赖。
// 后续如果需要增加按钮控制 Dashboard，再在这里扩展。
// ============================================================

static void button_event_task(void *arg)
{
    (void)arg;

    KeyEvent ev;

    while (1) {

        KeyState st =
            My_Button_GetEvent(
                &ev,
                portMAX_DELAY
            );

        if (st == KEY_NONE)
            continue;

        // 当前暂不处理普通按键事件。
        //
        // BOOT 5 秒事件由 main.cpp 的 boot_hold_task()
        // 独立处理。
        //
        // 后续如果要增加：
        //   KEY_SHORT
        //   KEY_LONG
        //   KEY_DOUBLE
        //
        // 可以在这里继续添加。
    }
}

void BtnPoll_StartBackground(void)
{
    static int started = 0;

    if (started)
        return;

    started = 1;

    xTaskCreate(
        button_event_task,
        "btn_evt",
        2048,
        NULL,
        2,
        NULL
    );
}