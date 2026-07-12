#ifndef MY_BUTTON_H
#define MY_BUTTON_H

#include <driver/gpio.h>
#include <esp_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

// 按键状态返回值
typedef enum {
    KEY_NONE   = 0,
    KEY_PRESSED,
    KEY_SHORT,
    KEY_LONG,
    KEY_DOUBLE
} KeyState;

// 初始化（注册引脚，配置上拉）
void My_Button_Init(void);

// 扫描指定引脚（低电平有效），返回按键事件
// 需要定期调用，调用周期建议 10ms
KeyState My_Button_Scan(gpio_num_t pin);

#ifdef __cplusplus
}
#endif

#endif
