// =====================================================================
//  My_Button.cpp  (中断 + esp_timer 实现)
//  ----------------
//  用 GPIO 边沿中断 + 四个 esp_timer 实现的多功能按键检测器。
//
//  事件：
//    KEY_PRESSED   按下沿（消抖后）
//    KEY_SHORT     短按松开（按下 < 300ms）
//    KEY_LONG      长按（按下 >= 1000ms，持续期间只报一次）
//    KEY_HOLD_5S   持续按住 >= 5000ms，只报一次（供 BOOT 5s 清 WiFi 用）
//    KEY_DOUBLE    双击（松开后 400ms 内再次完成一次短按）
//
//  ISR 只做最轻的事：
//    1. 读电平
//    2. 记一个 esp_timer_get_time() 时间戳
//    3. 启停 4 个 esp_timer（消抖 / 长按 / 5s hold / 双击窗口）
//  所有事件合成都在 esp_timer 回调里（任务上下文）。
//
//  多个 task 同时调用 My_Button_GetEvent() 是安全的：
//  每个调用方都在 xQueueReceive 上独立阻塞，事件会被先到的取走。
// =====================================================================

#include "My_Button.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

// ============== 时序参数（单位：毫秒） ==============
#define KEY_DEBOUNCE_MS 20
#define KEY_SHORT_MS 300
#define KEY_LONG_MS 1000
#define KEY_HOLD_MS 5000 // 持续按住 5s -> KEY_HOLD_5S
#define KEY_DOUBLE_MS 400

// 每个按键一个 Button_t
typedef struct
{
    gpio_num_t pin;

    // 状态机 (0=空闲, 1=按下已确认, 2=双击窗口)
    volatile uint8_t state;

    // 时间戳（ms）。volatile 是因为 ISR 和 timer 回调都会读写。
    volatile uint32_t press_start;
    volatile uint32_t release_at;
    volatile uint32_t long_fired_at; // 0=还没报长按; 否则记下时间以便去重
    volatile uint32_t hold_fired_at; // 0=还没报 5s hold; 否则记下时间以便去重

    // 四个 esp_timer
    esp_timer_handle_t t_debounce;
    esp_timer_handle_t t_long;
    esp_timer_handle_t t_hold;
    esp_timer_handle_t t_double;
} Button_t;

// 我们支持的引脚固定为 3 个，方便静态分配。
// 顺序：BOOT / back / settings
#define MY_BUTTON_NUM 3
static const gpio_num_t kPins[MY_BUTTON_NUM] = {
    GPIO_NUM_1,
    GPIO_NUM_2,
    GPIO_NUM_42,
};
static Button_t s_btns[MY_BUTTON_NUM] = {};

// 事件 FIFO：ISR / 定时器回调里 xQueueSendFromISR 投递，
// 业务层用 My_Button_GetEvent() 阻塞/非阻塞取。
static QueueHandle_t s_evt_queue = NULL;

// 日志 tag
static const char *TAG = "MyBtn";

// ---------------------------------------------------------------------
// 从 ISR / timer 回调里安全地投递一个事件。
// 阻塞 / 唤醒的工作交给 FreeRTOS 队列本身处理
// (xQueueSendFromISR 在队列满时返回 errQUEUE_FULL 而非阻塞)。
// ---------------------------------------------------------------------
static void IRAM_ATTR post_event_from_isr(gpio_num_t pin, KeyState type)
{
    if (!s_evt_queue)
        return;
    KeyEvent ev = {.pin = pin, .type = type};
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(s_evt_queue, &ev, &hp);
    (void)hp; // 队列满会丢事件，下次调试时再扩容量
}

// 从任务上下文投递（esp_timer 回调也是任务上下文）
static void post_event(gpio_num_t pin, KeyState type)
{
    if (!s_evt_queue)
        return;
    KeyEvent ev = {.pin = pin, .type = type};
    xQueueSend(s_evt_queue, &ev, 0);
}

// ---------------------------------------------------------------------
// 定时器回调（都在任务上下文执行）
// ---------------------------------------------------------------------

// 消抖定时器：到点说明电平已经稳了。
//   仍是低 -> 切换 state 到"已确认按下"，并发 KEY_PRESSED，
//            同时启动长按 / 5s hold 定时器。
//   已经高 -> 是抖动，啥也不做。
static void debounce_cb(void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= MY_BUTTON_NUM)
        return;
    Button_t *b = &s_btns[idx];
    if (gpio_get_level(b->pin) != 0)
        return; // 抖动丢弃
    b->state = 1;
    b->long_fired_at = 0;
    b->hold_fired_at = 0;
    esp_timer_start_once(b->t_long,
                         (uint64_t)KEY_LONG_MS * 1000ULL);
    esp_timer_start_once(b->t_hold,
                         (uint64_t)KEY_HOLD_MS * 1000ULL);
    post_event(b->pin, KEY_PRESSED);
}

// 长按定时器：到点说明已经按住 >= KEY_LONG_MS，发 KEY_LONG 一次。
static void long_cb(void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= MY_BUTTON_NUM)
        return;
    Button_t *b = &s_btns[idx];
    if (gpio_get_level(b->pin) != 0)
        return; // 早已松开
    if (b->long_fired_at != 0)
        return; // 已报过
    b->long_fired_at = esp_timer_get_time() / 1000;
    b->state = 1; // 仍在按住
    post_event(b->pin, KEY_LONG);
}

// 5s hold 定时器：到点说明已经按住 >= KEY_HOLD_MS，发 KEY_HOLD_5S 一次。
// 即使按键一直不松，也只报一次。
static void hold_cb(void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= MY_BUTTON_NUM)
        return;
    Button_t *b = &s_btns[idx];
    if (gpio_get_level(b->pin) != 0)
        return; // 早已松开
    if (b->hold_fired_at != 0)
        return; // 已报过
    b->hold_fired_at = esp_timer_get_time() / 1000;
    b->state = 1; // 仍在按住
    post_event(b->pin, KEY_HOLD_5S);
}

// 双击窗口定时器：松开后 KEY_DOUBLE_MS 内没收到第二次按下，
// 之前的"短按候选"就当单击 KEY_SHORT 发出。
static void double_window_cb(void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= MY_BUTTON_NUM)
        return;
    Button_t *b = &s_btns[idx];
    if (b->state == 2)
    {
        // 窗口内没等到第二次按下 -> 落定为单击
        b->state = 0;
        post_event(b->pin, KEY_SHORT);
    }
}

// ---------------------------------------------------------------------
// GPIO ISR（双边沿）。只更新时间戳 + 启停定时器，不合成事件。
// ---------------------------------------------------------------------
static void IRAM_ATTR gpio_isr(void *arg)
{
    int idx = (int)(intptr_t)arg;
    if (idx < 0 || idx >= MY_BUTTON_NUM)
        return;
    Button_t *b = &s_btns[idx];
    int level = gpio_get_level(b->pin);
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

    if (level == 0)
    {
        // 下降沿：按下
        // 重启 debounce 定时器（20ms 后再去确认电平）
        esp_timer_stop(b->t_debounce);
        esp_timer_start_once(b->t_debounce,
                             (uint64_t)KEY_DEBOUNCE_MS * 1000ULL);
        // 取消未到期的双击窗口（如果有的话说明上次是无效序列）
        esp_timer_stop(b->t_double);
        b->press_start = now_ms;
        // 重置长按 / 5s hold 标志
        b->long_fired_at = 0;
        b->hold_fired_at = 0;
    }
    else
    {
        // 上升沿：松开
        esp_timer_stop(b->t_debounce);
        esp_timer_stop(b->t_long);
        esp_timer_stop(b->t_hold);
        uint32_t dur = now_ms - b->press_start;

        if (dur >= (uint32_t)KEY_DEBOUNCE_MS)
        {
            // 有效一次按下-松开
            if (b->state == 2)
            {
                // 之前是"双击窗口已开放，等第二次按下"
                // 这次松开完成 -> 判定为双击
                b->state = 0;
                esp_timer_stop(b->t_double);
                post_event_from_isr(b->pin, KEY_DOUBLE);
                return;
            }
            if (dur < (uint32_t)KEY_SHORT_MS)
            {
                // 短按候选：开双击窗口
                b->state = 2;
                esp_timer_start_once(b->t_double,
                                     (uint64_t)KEY_DOUBLE_MS * 1000ULL);
            }
            else
            {
                // >= 300ms 直接当单击
                b->state = 0;
                post_event_from_isr(b->pin, KEY_SHORT);
            }
        }
        // dur < DEBOUNCE：抖动，什么都不发
    }
}

// ---------------------------------------------------------------------
// My_Button_Init()
// ---------------------------------------------------------------------
void My_Button_Init(void)
{
    // 1) 事件队列 (容量 8 已足够：3 个 pin x 短时间内不可能爆)
    s_evt_queue = xQueueCreate(8, sizeof(KeyEvent));
    configASSERT(s_evt_queue);

    // 2) 全局 ISR 服务（LOWMED 优先级，够用；不进 NMI，esp_timer 可用）
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "gpio_install_isr_service: %s",
                 esp_err_to_name(err));
        return;
    }

    // 3) 每个 pin：配 GPIO、装定时器、装 ISR
    for (int i = 0; i < MY_BUTTON_NUM; i++)
    {
        Button_t *b = &s_btns[i];
        b->pin = kPins[i];
        b->state = 0;
        b->press_start = 0;
        b->release_at = 0;
        b->long_fired_at = 0;
        b->hold_fired_at = 0;

        gpio_set_direction(b->pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(b->pin, GPIO_PULLUP_ONLY);

        // 四个 esp_timer
        const esp_timer_create_args_t args_deb = {
            .callback = debounce_cb,
            .arg = (void *)(intptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "btn_deb",
            .skip_unhandled_events = false,
        };

        const esp_timer_create_args_t args_long = {
            .callback = long_cb,
            .arg = (void *)(intptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "btn_long",
            .skip_unhandled_events = false,
        };

        const esp_timer_create_args_t args_hold = {
            .callback = hold_cb,
            .arg = (void *)(intptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "btn_hold",
            .skip_unhandled_events = false,
        };

        const esp_timer_create_args_t args_dbl = {
            .callback = double_window_cb,
            .arg = (void *)(intptr_t)i,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "btn_dbl",
            .skip_unhandled_events = false,
        };
        esp_timer_create(&args_deb, &b->t_debounce);
        esp_timer_create(&args_long, &b->t_long);
        esp_timer_create(&args_hold, &b->t_hold);
        esp_timer_create(&args_dbl, &b->t_double);

        // 双边沿触发
        gpio_set_intr_type(b->pin, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(b->pin, gpio_isr, (void *)(intptr_t)i);
    }

    ESP_LOGI(TAG, "My_Button initialized (ISR mode, %d pins)",
             MY_BUTTON_NUM);
}

// ---------------------------------------------------------------------
// My_Button_GetEvent()
// ---------------------------------------------------------------------
//   timeout_ms = 0   非阻塞
//   timeout_ms > 0   阻塞（FreeRTOS 队列自身支持阻塞）
//
// 多个 task 同时调用是安全的：每个调用方都在 xQueueReceive 上
// 独立阻塞，事件会被先到的那个取走。
// ---------------------------------------------------------------------
KeyState My_Button_GetEvent(KeyEvent *out_event, uint32_t timeout_ms)
{
    if (!s_evt_queue)
        return KEY_NONE;

    KeyEvent ev;
    if (xQueueReceive(s_evt_queue, &ev,
                      pdMS_TO_TICKS(timeout_ms)) == pdTRUE)
    {
        if (out_event)
            *out_event = ev;
        return ev.type;
    }
    return KEY_NONE;
}
