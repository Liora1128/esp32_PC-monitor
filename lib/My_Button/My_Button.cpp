// =====================================================================
//  My_Button.cpp
//  ----------------
//  一个用普通 GPIO + 软件轮询实现的轻量级按键事件检测器。
//
//  设计目标：
//    1. 不依赖任何外部中断（避免 ESP-IDF 中断服务里调用 esp_timer / 调度器
//       之类的限制），由调用方在主循环或 LVGL 定时回调里周期性调用
//       My_Button_Scan() 来"喂"按键状态机。
//    2. 同时支持单击 / 双击 / 长按三种事件，配合时钟菜单的 "短按 = 翻页、
//       长按 = 确认、双击 = 返回" 这种交互。
//    3. 按键低电平有效（按下 = 0，松开 = 1），硬件上电路上拉，按下时被拉低。
//    4. 最多支持 4 个按键（数组固定长度），超过会被忽略。
//
//  调用模型（典型）：
//     My_Button_Init();        // 上电初始化一次
//     while (1) {
//         KeyState s = My_Button_Scan(GPIO_NUM_1);
//         if (s != KEY_NONE) handle(s);   // 派发事件
//         vTaskDelay(pdMS_TO_TICKS(10));   // 10ms 扫描一次
//     }
//
//  注意：
//    - 扫描周期建议 10ms。扫描周期太短（如 < 2ms）会导致消抖窗口失效；
//      太长（如 > 50ms）会漏掉短按。
//    - GPIO 1 既是 BOOT 按钮又是这里的 "menu" 按钮；长按 BOOT 5s 触发
//      WifiProvision_ClearCredentials() 是 main.cpp 里另一个独立任务
//      直接读 GPIO 实现的，与本模块的按键事件机不冲突。
// =====================================================================

#include "My_Button.h"

// ============== 时序参数（单位：毫秒） ==============
//
// KEY_DEBOUNCE_MS  按下消抖窗口。
//                   机械按键按下瞬间会有几 ms 的电平抖动，这里用 20ms
//                   覆盖绝大多数按键的抖动时间。
//
// KEY_SHORT_MS     短按最大持续时间。
//                   按下后 < 300ms 就松开算一次"短按"，会触发 KEY_SHORT。
//                   注意：第一次进入 state=2 时立刻返回 KEY_PRESSED
//                   （"已按下"），真正"判定为短按"是在松开的瞬间。
//
// KEY_LONG_MS      长按判定时间。
//                   按下持续 >= 1000ms 时，扫描函数会返回 KEY_LONG。
//                   返回一次后进入 state=3 等待松开，期间不再产生事件。
//
// KEY_DOUBLE_MS    双击窗口。
//                   第一次短按松开后，400ms 内再次按下才算"双击"，返回
//                   KEY_DOUBLE；否则本次短按就当单击处理。
#define KEY_DEBOUNCE_MS   20
#define KEY_SHORT_MS      300
#define KEY_LONG_MS       1000
#define KEY_DOUBLE_MS     400

// ============== 内部按键状态结构 ==============
//
// 每个注册过的按键都有一份这样的状态机上下文。
// pin           : 该按键绑定的 GPIO 编号。
// last_level    : 上一次扫描读到的电平（0=按下 / 1=松开），用于状态机转移
//                 时的边沿判断（虽然 state 字段已经隐含了边沿信息，但保留
//                 这个字段便于在调试时把状态和电平一起打印出来）。
// press_start   : 当前这次"按下动作"的起始时间戳（ms）。state 0->1 转移
//                 时记录，用于计算持续时长。
// last_release  : 上一次"松开"事件的时间戳（ms），用于双击窗口判断。
// state         : 有限状态机的当前状态（取值 0/1/2/3/4，见下）。
// double_ready  : 第一次短按已经发生并松开，现在处于等待"第二次短按"阶段。
//                 等于 1 表示窗口开放；等于 0 表示窗口已关闭或已被消耗。
typedef struct {
    gpio_num_t  pin;
    int         last_level;
    uint32_t    press_start;
    uint32_t    last_release;
    uint8_t     state;
    uint8_t     double_ready;
} Button_t;

// 按键实例池（最多 4 个）。
// static 修饰保证这些状态只在本编译单元内可见，多个 .cpp 不会冲突。
static Button_t s_btns[4] = {};
// 已经注册（"用上"）的按键数量。每次 alloc_pin() 成功注册时 +1。
static uint8_t  s_btn_count = 0;

// ---------------------------------------------------------------------
// alloc_pin(pin)
// ---------------------------------------------------------------------
// 在 s_btns[] 里查找 pin 对应的 Button_t；找不到就分配一个新的。
// 返回值：
//   成功 -> 指向 s_btns[] 某一项的指针。
//   失败 -> NULL（已经注册满 4 个按键）。
//
// 为什么用"先查再分配"？
//   因为 My_Button_Scan() 会在每次被调用时都拿 pin 来查表，如果每次都
//   新建一份状态，按键的"上一次的时长"就丢了，长按/双击都判不出来。
//   所以同一个 pin 必须永远命中同一份 Button_t 上下文。
static Button_t* alloc_pin(gpio_num_t pin) {
    // 1) 先线性扫描查找已注册项（按键数量很少，O(n) 完全够用）
    for (int i = 0; i < s_btn_count; i++) {
        if (s_btns[i].pin == pin) return &s_btns[i];
    }
    // 2) 找不到且还没满 4 个 -> 注册新项
    if (s_btn_count < 4) {
        Button_t* b = &s_btns[s_btn_count++];
        b->pin = pin;
        // 初始默认电平为"松开"（1）。这一步其实是冗余的，因为全局
        // s_btns[] 是 0 初始化，state=0 才是真正的"空闲"标记，last_level
        // 只是辅助字段。但显式写出来便于阅读。
        b->last_level = 1;
        b->state = 0;
        b->double_ready = 0;
        b->press_start = 0;
        b->last_release = 0;
        return b;
    }
    // 3) 满了就放弃
    return NULL;
}

// ---------------------------------------------------------------------
// My_Button_Init()
// ---------------------------------------------------------------------
// 项目启动时调用一次：
//   - 把时钟菜单实际用到的 3 个 GPIO 配置为输入 + 内部上拉
//   - 强制把这 3 个 pin 注册进 s_btns[]，避免首次 My_Button_Scan() 时
//     因为还没 alloc_pin 走过而多花几个扫描周期。
//
// 引脚映射（与硬件 / PCB 实际对应）：
//   GPIO 1  -> menu     板子左下角 (注意：这也是 ESP32-S3 的 BOOT 脚，
//                       main.cpp 里独立任务用 5s 长按它来清 WiFi 凭据)
//   GPIO 2  -> back     板子中间
//   GPIO 42 -> settings 板子右下角
void My_Button_Init(void) {
    gpio_num_t pins[] = { GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_42 };
    for (int i = 0; i < 3; i++) {
        // 设为普通输入（不使能中断，软件轮询就够了）
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        // 打开内部上拉：按键另一端接 GND，未按下时由上拉保持高电平，
        // 按下时被外部下拉到低电平。
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
        // 强制注册（alloc_pin 内部会跳过已存在的）
        (void)alloc_pin(pins[i]);
        // 显式重置状态机：上电时假设按键处于"松开"状态。
        s_btns[i].last_level = 1;
        s_btns[i].state = 0;
        s_btns[i].double_ready = 0;
    }
}

// ---------------------------------------------------------------------
// My_Button_Scan(pin)
// ---------------------------------------------------------------------
// 周期性调用，参数 pin 是要查询的按键。
// 返回值：
//   KEY_NONE     没有任何新事件（绝大多数调用都返回这个）
//   KEY_PRESSED  刚刚确认一次有效按下（消抖通过），相当于"按下沿"事件
//   KEY_SHORT    松开，且本次按下时长 < 300ms（短按）
//   KEY_LONG     持续按下时长已 >= 1000ms（长按）
//   KEY_DOUBLE   在双击窗口内又完成一次短按（双击）
//
// 状态机 5 个状态（见 switch 里的注释）：
//   0 空闲       等待一个下降沿（按下）
//   1 消抖中     刚检测到按下，等 20ms 看看电平还稳不稳
//   2 已确认按下 等松开（短按）或超时（长按）
//   3 长按已报   长按事件已经发出去一次，等用户松开再回到 0
//   4 双击窗口   第一次短按已经松开，开放窗口等第二次按下
KeyState My_Button_Scan(gpio_num_t pin) {
    // 1) 拿到 pin 对应的状态机上下文（必要时 lazy 注册）
    Button_t* btn = alloc_pin(pin);
    if (!btn) return KEY_NONE;

    // 2) 读取当前电平和时间
    //    电平：0=按下 / 1=松开
    //    时间：esp_timer_get_time() 返回微秒，这里除以 1000 换成毫秒，
    //    方便和 KEY_*_MS 这些宏比较。
    int level = gpio_get_level(pin);
    uint32_t now = esp_timer_get_time() / 1000;

    switch (btn->state) {

        // ----- 状态 0：空闲，等待按下 -----
        // 行为：
        //   看到电平变低（按下沿）就记录时间戳，进入消抖状态。
        //   其它情况保持空闲。
        case 0:
            if (level == 0) {
                btn->press_start = now;   // 记下"按下开始"的时间
                btn->last_level = 0;      // 记录当前电平
                btn->state = 1;           // 切到消抖状态
            }
            break;

        // ----- 状态 1：消抖中 -----
        // 20ms 内电平仍然稳定为低 -> 消抖通过，进入状态 2，同时返回
        // KEY_PRESSED 让上层可以"按下立刻响应"（比如开始屏幕动画）。
        // 20ms 内电平已经变回高 -> 是抖动，回到状态 0 当无事发生。
        case 1:
            if (level == 0) {
                if (now - btn->press_start >= KEY_DEBOUNCE_MS) {
                    btn->state = 2;
                    return KEY_PRESSED;
                }
                // 还没到 20ms 继续保持现状
            } else {
                // 抖动：按下后又立即松开
                btn->state = 0;
            }
            break;

        // ----- 状态 2：已确认按下，等松开或长按 -----
        // 这是状态机里最复杂的一段。逻辑：
        //   - 如果检测到松开：
        //       * 持续 < 300ms  -> 短按候选，进入双击判定
        //       * 持续 >= 300ms -> 已经超过短按阈值，但因为短按判定只在
        //         松开瞬间做，所以这里直接当短按返回
        //   - 如果还在按住：
        //       * 持续 >= 1000ms -> 长按，进入状态 3 等松开
        case 2:
            if (level == 1) {
                uint32_t dur = now - btn->press_start;

                if (dur < KEY_SHORT_MS) {
                    // 短按：先看双击窗口是否还开着
                    if (btn->double_ready && (now - btn->last_release) <= KEY_DOUBLE_MS) {
                        // 窗口内又完成了一次短按 -> 判定为双击
                        btn->double_ready = 0;
                        btn->last_release = now;
                        btn->state = 0;
                        return KEY_DOUBLE;
                    } else {
                        // 第一次短按：标记窗口开放，等可能的第二次短按
                        btn->double_ready = 1;
                        btn->last_release = now;
                        btn->state = 4;   // 进入"等第二次按下"状态
                        return KEY_SHORT;
                    }
                }
                // 持续时间 >= 300ms：直接当单击（不做双击判定）
                btn->double_ready = 0;
                btn->last_release = now;
                btn->state = 0;
                return KEY_SHORT;
            } else {
                // 还在按着：判断是否到了长按阈值
                if (now - btn->press_start >= KEY_LONG_MS) {
                    btn->state = 3;
                    return KEY_LONG;
                }
            }
            break;

        // ----- 状态 3：长按已报，等松开 -----
        // 长按事件在 state=2 里只发一次，进入 state=3 后等用户松开，
        // 中间持续按着不再产生任何事件。
        case 3:
            if (level == 1) {
                btn->state = 0;   // 松开，回到空闲
            }
            break;

        // ----- 状态 4：双击窗口已开放，等第二次按下 -----
        // 两种退出路径：
        //   - 检测到新一次按下（电平 0）-> 回到状态 1 重新消抖
        //   - 双击窗口超时（> 400ms 没有第二次按下）-> 回到空闲
        case 4:
            if (level == 0) {
                // 第二次按下开始：清掉双击窗口标志，重置消抖起点
                btn->double_ready = 0;
                btn->press_start = now;
                btn->state = 1;
            } else if (now - btn->last_release > KEY_DOUBLE_MS) {
                // 超时：本轮短按就当单击处理（不再回溯补发 KEY_SHORT，
                // 因为 KEY_SHORT 在进入 state=4 那一刻已经发过了）
                btn->state = 0;
            }
            break;
    }

    // 大部分扫描周期都走到这里：状态没变、没新事件。
    return KEY_NONE;
}
