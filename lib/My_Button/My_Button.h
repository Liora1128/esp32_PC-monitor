#ifndef MY_BUTTON_H
#define MY_BUTTON_H

#include <driver/gpio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 按键事件类型
typedef enum {
    KEY_NONE   = 0,
    KEY_PRESSED,  // 确认按下沿（消抖通过后立即产生）
    KEY_SHORT,    // 短按松开（<300ms）
    KEY_LONG,     // 长按（按下持续 >= 1000ms，只报一次）
    KEY_DOUBLE,   // 双击（双击窗口内第二次短按）
    KEY_HOLD_5S,  // 持续按住 >= 5000ms，只报一次（用于 BOOT 5s 清 WiFi）
} KeyState;

// 一个具体的事件（哪个 pin + 哪种类型）
typedef struct {
    gpio_num_t pin;
    KeyState   type;
} KeyEvent;

// 初始化：把三个 GPIO (1/2/42) 配成输入+上拉，安装 ISR 服务，
// 给每个 pin 注册双边沿 ISR，创建所需要的 esp_timer。
// 必须在调用 My_Button_GetEvent() 之前调用一次。
void My_Button_Init(void);

// 从事件 FIFO 里取一个事件。
//   timeout_ms = 0   -> 非阻塞，没事件立刻返回 KEY_NONE
//   timeout_ms > 0   -> 阻塞等待直到拿到事件或超时
//
// 返回值：拿到的 KeyState（同时 out_event 会被填充）。
// 任何非 KEY_NONE 的返回值都对应一个真实事件。
KeyState My_Button_GetEvent(KeyEvent *out_event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
