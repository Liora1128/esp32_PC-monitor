#include "My_Button.h"

// ============== 配置 ==============
#define KEY_DEBOUNCE_MS   20
#define KEY_SHORT_MS      300
#define KEY_LONG_MS       1000
#define KEY_DOUBLE_MS     400

// ============== 内部状态 ==============
typedef struct {
    gpio_num_t  pin;
    int         last_level;
    uint32_t    press_start;
    uint32_t    last_release;
    uint8_t     state;
    uint8_t     double_ready;
} Button_t;

static Button_t s_btns[4] = {};
static uint8_t  s_btn_count = 0;

// 查找或注册引脚
static Button_t* alloc_pin(gpio_num_t pin) {
    for (int i = 0; i < s_btn_count; i++) {
        if (s_btns[i].pin == pin) return &s_btns[i];
    }
    if (s_btn_count < 4) {
        Button_t* b = &s_btns[s_btn_count++];
        b->pin = pin;
        b->last_level = 1;
        b->state = 0;
        b->double_ready = 0;
        b->press_start = 0;
        b->last_release = 0;
        return b;
    }
    return NULL;
}

void My_Button_Init(void) {
    // Pre-register the three buttons used by the clock menu.
    // GPIO 1 = menu (bottom-left), GPIO 2 = back, GPIO 42 = settings (bottom-right)
    gpio_num_t pins[] = { GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_42 };
    for (int i = 0; i < 3; i++) {
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
        // force registration
        (void)alloc_pin(pins[i]);
        s_btns[i].last_level = 1;
        s_btns[i].state = 0;
        s_btns[i].double_ready = 0;
    }
}

KeyState My_Button_Scan(gpio_num_t pin) {
    Button_t* btn = alloc_pin(pin);
    if (!btn) return KEY_NONE;

    int level = gpio_get_level(pin);
    uint32_t now = esp_timer_get_time() / 1000;

    switch (btn->state) {

        // ----- 0: 空闲，等按下 -----
        case 0:
            if (level == 0) {
                btn->press_start = now;
                btn->last_level = 0;
                btn->state = 1;
            }
            break;

        // ----- 1: 消抖中 -----
        case 1:
            if (level == 0) {
                if (now - btn->press_start >= KEY_DEBOUNCE_MS) {
                    btn->state = 2;
                    return KEY_PRESSED;
                }
            } else {
                btn->state = 0;
            }
            break;

        // ----- 2: 已按下，等松开或长按 -----
        case 2:
            if (level == 1) {
                uint32_t dur = now - btn->press_start;

                if (dur < KEY_SHORT_MS) {
                    if (btn->double_ready && (now - btn->last_release) <= KEY_DOUBLE_MS) {
                        btn->double_ready = 0;
                        btn->last_release = now;
                        btn->state = 0;
                        return KEY_DOUBLE;
                    } else {
                        btn->double_ready = 1;
                        btn->last_release = now;
                        btn->state = 4;
                        return KEY_SHORT;
                    }
                }
                btn->double_ready = 0;
                btn->last_release = now;
                btn->state = 0;
                return KEY_SHORT;
            } else {
                if (now - btn->press_start >= KEY_LONG_MS) {
                    btn->state = 3;
                    return KEY_LONG;
                }
            }
            break;

        // ----- 3: 长按已报，等松开 -----
        case 3:
            if (level == 1) {
                btn->state = 0;
            }
            break;

        // ----- 4: 等双击窗口 -----
        case 4:
            if (level == 0) {
                btn->double_ready = 0;
                btn->press_start = now;
                btn->state = 1;
            } else if (now - btn->last_release > KEY_DOUBLE_MS) {
                btn->state = 0;
            }
            break;
    }

    return KEY_NONE;
}
