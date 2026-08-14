# ESP32-S3 PC Monitor Dashboard

一个基于 **ESP32-S3** + **1.3" 240×240 ST7789 TFT** 的迷你桌面监视器,在小小的屏幕上实时显示电脑的 **CPU / 内存 / 温度 / 频率 / 网速 / 显卡 / 磁盘 / 启动时间** 等状态。

PC 端通过 UDP 广播把监控数据投递给 ESP32,ESP32 在局域网里开机即用,无需任何额外软件。首次使用只需手机连一下 ESP32 发出的小热点,在弹出的网页里填好家里 Wi-Fi 即可。

> **主控**:  ESP32-S3 (4D Systems gen4-r8n16 评估板)
> **显示**:  1.3" ST7789 SPI LCD (240×240, BGR)
> **UI**:    LVGL 8.3 + LovyanGFX
> **系统**:  ESP-IDF (通过 PlatformIO)

---

## 📑 目录

- [✨ 功能特性](#-功能特性)
- [🖼️ 效果展示](#-效果展示)
- [🔌 硬件准备与接线](#-硬件准备与接线)
- [📂 项目结构](#-项目结构)
- [🛠️ 软件环境](#-软件环境)
- [🚀 编译与烧录](#-编译与烧录)
- [📶 第一次使用:Wi-Fi 配网](#-第一次使用wi-fi-配网)
- [🖥️ PC 端:如何把监控数据发给 ESP32](#-pc-端如何把监控数据发给-esp32)
- [📡 通信协议](#-通信协议)
- [🧰 进阶:BOOT 键 5 秒清 Wi-Fi](#-进阶boot-键-5-秒清-wi-fi)
- [❓ 常见问题 FAQ](#-常见问题-faq)
- [🧾 开源协议](#-开源协议)

---

## ✨ 功能特性

- 🎯 **CPU 实时监控** — 占用率圆环 + 大字百分比 + 当前频率 (GHz)
- 🌡️ **CPU 温度** — PC 上报时显示,无数据自动隐藏
- ⏱️ **Uptime / CPU TEMP 自适应显示**:
  - PC 同时上报两个字段 → **每 3 秒** 自动切换显示
  - PC 只报一个 → 一直显示对应字段
  - 都没有 → 显示 `SYSTEM --`
- 🧠 **内存条** — 百分比条 + 已用/总量 (GB)
- 🌐 **网络上下行速率** — 自动单位 (B/s / KB/s / MB/s)
- 🎮 **GPU 占用 / 显存 / 温度** (取决于 PC 端实现)
- 💾 **磁盘读写速率** (KB/s)
- 📺 **240×240 深色卡片 UI**,LVGL 8.3 绘制,1 Hz 定时刷新
- 📶 **Wi-Fi Captive Portal 配网** — 扫描附近热点 → 选择 → 输入密码 → 自动校验 → 自动重启
- 🔁 **热插拔 PC** — PC 离线 10 秒后,Dashboard 自动标记 `SYSTEM --`,等待新 PC 上线;新 PC 用 mDNS 发现 ESP32 后即可恢复数据
- 📡 **mDNS 服务公告** — ESP32 启动后以 `pcmonitor.local` 身份在局域网内广播 `_pcmonitor._udp` 服务(端口 9999),PC 端可通过域名自动找到设备,无需查 IP
- 🧹 **BOOT 5 秒清 Wi-Fi** — 免重刷,随时重置

---

## 🖼️ 效果展示

主界面(PC 正常发送全部字段时):

```
┌────────────────────────────┐
│   ╭───────╮   ┌────────┐   │
│  ╱   45    ╲  │UPTIME  │   │   ← 右上角标题动态切换:
│ │           │ │ 2d 04h │   │     UPTIME / CPU TEMP / SYSTEM
│  ╲         ╱  ├────────┤   │
│   ╰───────╯   │CPU FREQ│   │
│   CPU %       │ 2.72GHz│   │
│               └────────┘   │
│ ┌──────────────────────┐   │
│ │ RAM ████████░░  62%  │   │
│ └──────────────────────┘   │
│ ┌──────────┐ ┌──────────┐  │
│ │ ^ UP     │ │ v DOWN   │  │
│ │ 234 KB/s │ │ 1.2 MB/s │  │
│ └──────────┘ └──────────┘  │
└────────────────────────────┘
```

配网阶段(进入 AP 模式后):

```
┌────────────────────────────┐
│                            │
│      PCMonitor-Setup       │
│      Wi-Fi Setup           │
│                            │
│      Open 192.168.4.1      │
│      to configure Wi-Fi    │
│                            │
│      ● ● ● (spinner)       │
│                            │
└────────────────────────────┘
```

---

## 🔌 硬件准备与接线

### 物料清单 (BOM)

| 数量 | 元件                       | 备注                                 |
| ---- | -------------------------- | ------------------------------------ |
| 1    | ESP32-S3 开发板            | 推荐 4D Systems ESP32-S3 Gen4 R8N16  |
| 1    | 1.3 寸 ST7789 TFT 模块     | 240×240, SPI 接口, **BGR 顺序**      |
| 1    | 3 个轻触按键 (可选)        | BOOT(GPIO1) / Back(GPIO2) / Set(GPIO42) |
| 1    | USB Type-C 数据线          | 用于烧录 + 供电 (5V/500 mA 即可)     |

### 引脚对照表 (ESP32-S3 ↔ 1.3" ST7789)

| TFT 信号        | ESP32-S3 GPIO | 说明                          |
| --------------- | ------------- | ----------------------------- |
| `VCC`           | **3V3**       | ⚠️ 务必使用 3.3 V,不要接 5 V |
| `GND`           | `GND`         |                               |
| `SCL` / `SCLK`  | **GPIO12**    | SPI 时钟                      |
| `SDA` / `MOSI`  | **GPIO11**    | SPI 数据 (MISO 未用)          |
| `CS`            | **未连接 (CS=-1)** | 通过 DC 区分命令/数据,代码已写死 |
| `DC` / `RS`     | **GPIO9**     | 数据/命令选择                 |
| `RST`           | **GPIO8**     | LCD 复位                      |
| `BL` / `LED`    | **GPIO45**    | 背光 PWM (LEDC,13-bit,1 kHz)  |

> ⚠️ **重要提示**:
> - `RST` 必须接到 GPIO8,否则 LCD 初始化时序会失败,屏幕永远不亮
> - `BL` 接到 GPIO45 是因为代码里固定使用 `LEDC_CHANNEL_0` + `LEDC_TIMER_0`
> - `CS` 引脚不需要接,代码里已用 DC 模拟片选

### 接线原理图 (ASCII)

```
                ┌────────────────────────┐
                │      ESP32-S3          │
                │                        │
       3V3 ─────┤                        ├──── GPIO8  ── RST ──┐
                │                        │                     │
       GND ─────┤                        ├──── GPIO9  ── DC  ─┤
                │                        │                     │
                │                        ├──── GPIO11 ── SDA ──┤
                │                        │                     ├──► ST7789 TFT
                │                        ├──── GPIO12 ── SCL ──┤
                │                        │                     │
                │                        ├──── GPIO45 ── BL  ──┘
                └────────────────────────┘
```

### 可选按键 (用于 BOOT 5 秒清 Wi-Fi)

| 按键   | GPIO   | 行为                                       |
| ------ | ------ | ------------------------------------------ |
| BOOT   | GPIO1  | 长按 **≥ 5 秒** → 清除保存的 Wi-Fi 并重启  |
| BACK   | GPIO2  | 预留 (目前无功能)                          |
| SET    | GPIO42 | 预留 (目前无功能)                          |

按键另一脚接 GND,代码内部已使能内部上拉,**无需外接上拉电阻**。

### 供电

- 通过 USB Type-C 5 V 直接供电即可 (板载 LDO 转 3.3 V)。
- TFT 工作电流约 40 mA,ESP32-S3 峰值约 240 mA,**5 V / 500 mA 完全够用**。

---

## 📂 项目结构

```
ESP32S3/
├── platformio.ini            # PlatformIO 配置 (espidf + 4d_systems 板)
├── sdkconfig*                # ESP-IDF Kconfig 配置
├── partitions.csv            # 自定义分区表
├── lv_conf.h                 # LVGL 配置 (颜色深度 / 缓冲 / 主题)
├── src/
│   ├── main.cpp              # app_main: 屏幕 + LVGL + 网络任务
│   └── CMakeLists.txt
├── include/                  # 公共头文件
├── lib/
│   ├── 1.3TFT/               # LovyanGFX 配置 + ST7789 初始化 + 背光 PWM
│   ├── My_Button/            # GPIO 中断 + esp_timer 按键事件库
│   ├── NetSync/              # Wi-Fi 状态机 + UDP 业务 + 配网
│   │   ├── NetSync.cpp
│   │   ├── NetSync.h
│   │   ├── wifi_provision.cpp    # 内嵌 HTML 配网页面 (Captive Portal)
│   │   └── wifi_provision.h
│   ├── Provision_UI/         # 配网阶段 LVGL 界面
│   ├── Dashboard/            # 主监控面板 (LVGL)
│   ├── LightSensor/          # (可选) 光敏电阻,目前未启用
│   └── RtosTasks/
│       └── freertos.cpp/.h   # FreeRTOS 后台任务封装
└── test/                     # 单元测试占位
```

每个 `lib/<Name>/` 子目录都是一个 PlatformIO 静态库,自动包含在编译中。

### 主要库说明

| 库名 | 作用 |
| ---- | ---- |
| `1.3TFT`     | SPI 总线配置 (27 MHz 写 / 16 MHz 读)、ST7789 复位时序、背光 LEDC PWM |
| `My_Button`  | 3 个 GPIO 按键的事件队列,支持短按 / 长按 / 双击 / 5 秒 hold |
| `NetSync`    | Wi-Fi 连接状态机、mDNS 服务公告 + UDP 9999 收发、JSON 解析 |
| `wifi_provision` | AP 模式 + 内嵌 HTML 配网页面 + NVS 凭据存储 |
| `Provision_UI` | 配网阶段的 LVGL 界面 (提示 SSID / 连接中 / 失败 / 成功) |
| `Dashboard`  | 主监控 UI (1 Hz tick 刷新所有数字) |
| `LightSensor`| (预留) GPIO1 ADC 读光敏,目前未接入主流程 |
| `RtosTasks`  | FreeRTOS 任务封装 (按键轮询后台) |

---

## 🛠️ 软件环境

### 1. 安装 PlatformIO Core / IDE

两种方式任选:

- **VSCode 插件**: 搜索安装 `PlatformIO IDE`
- **命令行**:

  ```bash
  python -m pip install -U platformio
  ```

### 2. ESP-IDF 工具链

PlatformIO 会在第一次构建时自动下载:

- `xtensa-esp32s3-elf-gcc`
- ESP32-S3 Python 工具链
- ESP-IDF 5.x 框架

> 如果你在中国大陆,首次下载会比较慢,建议提前配置镜像或代理。

### 3. USB 驱动

- 大多数 4D Systems 评估板使用 **USB-NCDC**,免驱
- 其他常见 ESP32-S3 板可能需要 **CP210x / CH340 / CH343** 之一,自行安装对应驱动

### 4. 字体 (LVGL Montserrat)

代码里使用了 `lv_font_montserrat_14 / 16 / 48`,这些字体随 LVGL 一起发布,无需额外安装。

---

## 🚀 编译与烧录

### 1. 克隆仓库

```bash
git clone https://github.com/<your-name>/<repo-name>.git
cd <repo-name>
```

### 2. 用 VSCode 打开

- VSCode → PlatformIO → Open Project → 选择此目录
- 左下角环境选择 `4d_systems_esp32s3_gen4_r8n16`
- 点击 ✓ (Build) → → (Upload) → 🔌 (Monitor)

### 3. 用命令行

```bash
# 编译
pio run

# 烧录 (默认串口)
pio run --target upload

# 烧录 + 监视串口
pio device monitor -b 115200
```

> 默认 upload 端口由 PlatformIO 自动检测。若有多个串口,可显式指定:
>
> ```bash
> pio run --target upload --upload-port COM7
> ```

第一次构建会下载 LVGL / LovyanGFX / ESP-IDF,大约 5–15 分钟;后续增量编译几秒到十几秒。

### 4. 烧录后第一次启动

1. 插入 USB,屏幕先短暂黑屏 (背光关闭避免花屏)
2. 屏幕出现 `PCMonitor-Setup / Wi-Fi Setup / Open 192.168.4.1`
3. 串口输出 `wifi_prov: Provision started` 等日志

---

## 📶 第一次使用:Wi-Fi 配网

设备开机后如果检测不到已保存的 Wi-Fi,会自动进入 **AP 配网模式**:

1. **手机/电脑 Wi-Fi 列表** 里找到 SSID:

   ```
   PCMonitor-Setup
   ```

2. 连接此热点 (无密码)。

3. 弹出配网页面 (手机一般会自动弹出,电脑需要手动打开浏览器访问 `192.168.4.1`):

   - 页面会自动扫描附近 Wi-Fi
   - 选择你的 2.4G Wi-Fi,输入密码
   - 点击 **Connect**

4. ESP32 会:
   - 把凭据存进 NVS (掉电不丢失)
   - 自动尝试连接你的 Wi-Fi
   - **30 秒内**验证连接
   - 成功后自动重启

5. 重启后:
   - 屏幕切到 Dashboard (显示 CPU 环 / RAM 条 / 网速等)
   - 串口打印 `IP=192.168.x.x`

> ⚠️ **注意**: ESP32-S3 **只支持 2.4 GHz Wi-Fi**,5 GHz 热点会扫描不到。

### 配网 HTTP API (供高级用户)

| 方法 | 路径                       | 说明                                          |
| ---- | -------------------------- | --------------------------------------------- |
| GET  | `/`                        | 配网页面 (HTML)                               |
| GET  | `/scan`                    | 扫描附近 Wi-Fi (JSON)                         |
| POST | `/save`                    | `ssid=...&pass=...` 保存并尝试连接            |
| GET  | `/status`                  | 轮询当前配网阶段 (`verifying` / `fail` / `saved`) |
| GET  | `/restart`                 | 重启设备                                      |
| GET  | `/hotspot_detect.html`     | 用于 Captive Portal 探测重定向                |

---

## 🖥️ PC 端:如何把监控数据发给 ESP32

本项目只负责 ESP32 这边的接收与显示,**PC 端发什么数据、按什么频率发,完全由你自己决定**。
下面给出一个最小可用的 Python 示例。

### 1. 准备依赖

```bash
pip install psutil pynvml
```

- `psutil` 用来读 CPU / RAM / 磁盘 / 网速 / 启动时间
- `pynvml` 可选,用来读 NVIDIA GPU (只有 NVIDIA 卡能读)

### 2. Python 示例

```python
"""
PC 端示例:每秒一次,把监控数据以 JSON 形式
通过 UDP 发到 ESP32 的 9999 端口。

ESP32 的地址可以通过 mDNS 直接拿到:
  - 如果 PC 支持 mDNS 解析:`pcmonitor.local`(推荐)
  - 不支持时回退到查路由器 DHCP 客户端列表
  - 或者看 ESP32 串口日志里的 IP=...
"""

import json
import socket
import time

import psutil

# 用 mDNS 域名,失败时再退回 IP
ESP32_HOST = "pcmonitor.local"  # ← ESP32 启动后 mDNS 公告的主机名
ESP32_PORT = 9999

> 💡 Windows 自带的 `getaddrinfo` 在某些老版本上不解析 `.local`,如果解析失败可以把 `ESP32_HOST` 换成路由器分配的 IP。


def build_payload():
    cpu  = psutil.cpu_percent(interval=None)
    freq = psutil.cpu_freq()                # MHz
    mem  = psutil.virtual_memory()
    net  = psutil.net_io_counters()
    disk = psutil.disk_io_counters()
    boot = psutil.boot_time()

    payload = {
        "cpu":   int(cpu),
        "freq":  round((freq.current or 0) / 1000.0, 2),   # GHz
        "ram":   int(mem.percent),
        "ram_u": round(mem.used  / 1024**3, 2),
        "ram_t": round(mem.total / 1024**3, 2),
        "up":    net.bytes_sent,
        "down":  net.bytes_recv,
        "up_t":  net.bytes_sent,
        "dn_t":  net.bytes_recv,
        "d_r":   disk.read_bytes  // 1024,
        "d_w":   disk.write_bytes // 1024,
        "uptime": int(time.time() - boot),
        # 如果你用 pynvml 读了 NVIDIA GPU,可以再加上:
        # "cpu_t":  current_cpu_temp,
        # "gpu":    gpu_load,
        # "gpu_t":  gpu_temp,
        # "gpu_m":  gpu_mem_pct,
    }
    return payload


def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # 第一次采样必须先 psutil.cpu_percent() 一下,
    # 否则第一次读到的 cpu 永远为 0
    psutil.cpu_percent(interval=None)

    while True:
        try:
            data = json.dumps(build_payload())
            sock.sendto(data.encode("utf-8"),
                        (ESP32_HOST, ESP32_PORT))
        except Exception as e:
            print("send error:", e)
        time.sleep(1)


if __name__ == "__main__":
    main()
```

> **提示**:字段名要严格匹配小写,详见下一节的 [通信协议](#-通信协议)。
> 缺的字段 ESP32 不会报错,只是 Dashboard 上对应位置显示 `--`。

### 3. 路由 / 防火墙

- 电脑和 ESP32 **必须在同一个 2.4G Wi-Fi 子网**。
- 如果电脑有防火墙,首次请放行 Python 的 UDP 出站。
- macOS / Linux 一般无需配置;Windows Defender 首次会弹窗询问。

### 4. 想开机自启?

- **Windows**: 把脚本保存成 `.pyw`,放到 `shell:startup` 文件夹里
- **macOS**: 用 `launchctl` 写一个 LaunchAgent
- **Linux**: 写一个 systemd user service

---

## 📡 通信协议

### 端口分配

| 端口 | 方向            | 用途                                              |
| ---- | --------------- | ------------------------------------------------- |
| 9999 | PC  → ESP32     | PC 推送监控 JSON,频率任意 (推荐 1 Hz)            |
| 5353 | ESP32 ↔ PC (mDNS) | ESP32 以 `pcmonitor.local` 公告 `_pcmonitor._udp` |
| 80   | 手机 → ESP32 AP | Captive Portal 配网页面 (仅 AP 模式下)            |

### 1. mDNS 服务发现 (ESP32 → 局域网)

ESP32 Wi-Fi 连上后,会以主机名 `pcmonitor` 启动 mDNS 客户端,并公告一个服务:

- 实例名: `pcmonitor`
- 服务类型: `_pcmonitor._udp.local`
- 端口: `9999`

PC 端用支持 mDNS 的方式查询 `_pcmonitor._udp.local` 即可拿到 ESP32 的 IP,无需在路由器里查 DHCP 列表。
推荐直接用域名 `pcmonitor.local:9999` 作为目标地址(Windows 10+ / macOS / Linux 默认都支持)。

### 2. 监控 JSON (PC → ESP32)

字段全部小写,可以是任意子集,**缺失字段不会被当作错误**,Dashboard 会保留上一次的值并标记 `has_xxx = false`。

| 字段      | 类型    | 含义                              | 单位      |
| --------- | ------- | --------------------------------- | --------- |
| `cpu`     | int     | CPU 总占用率                      | 0–100     |
| `freq`    | float   | CPU 频率                          | GHz       |
| `freq100` | int     | CPU 频率 ×100 (优先于 `freq`)     | —         |
| `cpu_t`   | int     | CPU 温度                          | °C        |
| `ram`     | int     | RAM 占用率                        | 0–100     |
| `ram_u`   | float   | RAM 已用                          | GB        |
| `ram_t`   | float   | RAM 总量                          | GB        |
| `swap`    | int     | Swap 占用率                       | 0–100     |
| `up`      | int     | 当前发送字节数 (累积)             | bytes     |
| `down`    | int     | 当前接收字节数 (累积)             | bytes     |
| `up_t`    | int     | 发送字节总数                      | bytes     |
| `dn_t`    | int     | 接收字节总数                      | bytes     |
| `gpu`     | int     | GPU 占用率                        | 0–100     |
| `gpu_m`   | int     | GPU 显存占用                      | 0–100     |
| `gpu_t`   | int     | GPU 温度                          | °C        |
| `d_r`     | int     | 磁盘读速率                        | KB/s      |
| `d_w`     | int     | 磁盘写速率                        | KB/s      |
| `nic`     | string  | 网卡名 (例如 "eth0")              | —         |
| `uptime`  | int     | 系统启动时长                      | 秒        |
| `cores`   | array   | 每个核心的占用率 (最多 8 个)      | 0–100     |

#### 最小 JSON 示例

```json
{"cpu": 23, "freq": 2.72, "ram": 41, "uptime": 86400}
```

ESP32 会:

- 在圆环里显示 `23`
- CPU FREQ 卡显示 `2.72 GHz`
- RAM 条显示 `41%`
- UPTIME 显示 `1d 00:00`
- 网速、磁盘等其它字段保持上次值并显示 `--`

#### 完整 JSON 示例

```json
{
  "cpu":   45,
  "freq":  2.72,
  "cpu_t": 56,
  "ram":   62,
  "ram_u": 9.8,
  "ram_t": 16.0,
  "swap":  3,
  "up":    12345678,
  "down":  987654321,
  "up_t":  12345678,
  "dn_t":  987654321,
  "gpu":   12,
  "gpu_m": 35,
  "gpu_t": 48,
  "d_r":   128,
  "d_w":   64,
  "uptime": 173456,
  "nic":   "wlan0",
  "cores": [40, 50, 45, 48]
}
```

### 3. JSON 解析容错

ESP32 端用 `strstr + atoi` 风格解析:

- 字段不存在 → 保留上一次的值
- 字段值为空 → 保留上一次的值
- 字段值为非数字 → 返回 0
- 整段 JSON 损坏 → 整包丢弃,不变动 `s_data`

所以你可以随意做字段删减,**无需保证每次都发完整字段集**。

---

## 🧰 进阶:BOOT 键 5 秒清 Wi-Fi

任何时候,只要 **按住 BOOT 键 (GPIO1) ≥ 5 秒**,ESP32 会:

1. 打印 `BOOT held for 5 seconds`
2. 调用 `WifiProvision_ClearCredentials()` 清掉 NVS 里的 SSID / 密码
3. 自动 `esp_restart()`

重启后会自动进入 **AP 配网模式**,你可以重新选 Wi-Fi。

> 这是清理配置最安全的办法,**比 `pio run --target erase_flash` 更安全** —— 后者会连 NVS 一起擦掉。

---

## ❓ 常见问题 FAQ

### Q1: 烧录后屏幕一直黑屏?

- 检查 `RST` 是否真的接到 GPIO8
- 检查 `BL` 是否接到 GPIO45
- 串口看有没有 `display initialized` 日志
- 如果卡在 `wifi_prov: Provision started` 之前的 log,可能是 LVGL 字体没编译成功,试试 `pio run -t clean`

### Q2: 配网页面打不开?

- 确认手机已经连上 `PCMonitor-Setup` 这个 SSID
- 部分安卓机会把 `192.168.4.1` 当作 "无互联网" 自动断网,需要手动点 "保持连接"
- iPhone 一般会自动弹出 Captive Portal 页面
- 实在不行,在浏览器手动输入 `192.168.4.1`

### Q3: 配网成功但 Dashboard 没数据?

- 打开串口日志,看有没有:
  ```
  UDP 9999 listening
  ```
  如果没有,可能是 Wi-Fi 路由器的 AP 隔离或客户端隔离,关闭它
- 用手机 ping ESP32 IP 看能不能通
- 检查 PC 防火墙是否拦截了 Python 的 UDP 出站
- 看 PC 脚本是不是真的发到 `9999` 端口,可以用 `tcpdump -i any udp port 9999` (Linux/macOS) 抓包

### Q4: 网速显示的数值不动?

网速字段 `up` / `down` 是 **累积字节数**,ESP32 内部自动做差值。
如果你 PC 端发的是 **每秒瞬时值**(比如 234),显示就会跳变非常大。
正确做法:发送 `psutil.net_io_counters().bytes_sent/recv` 的**累计值**,ESP32 端会按 1 秒差分得到速率。

### Q5: 显示器发黄/发蓝/方向不对?

修改 `lib/1.3TFT/1_3TFT.cpp` 里的:

```cpp
cfg.rgb_order = false;          // true / false 切换 BGR / RGB
invertDisplay(true);            // 颜色反转
cfg.offset_x = 0;               // 水平偏移
cfg.offset_y = 0;               // 垂直偏移
```

不同批次的 ST7789 模组 `rgb_order` 和 `invertDisplay` 可能相反,试一下两种组合。

### Q6: 编译时找不到 `lv_font_montserrat_14`?

确认 `lv_conf.h` 里:

```c
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_48 1
```

如果之前关掉了,改成 `1` 然后重新编译。

### Q7: 怎么在 Dashboard 上加新字段?

1. 在 `lib/NetSync/NetSync.h` 的 `NetSync_Data` 加新字段
2. 在 `lib/NetSync/NetSync.cpp::parse_json_line()` 里加 `json_int(...)` 解析
3. 在 `lib/Dashboard/Dashboard.cpp::tick_cb()` 里把字段读进静态变量
4. 在 `build_page_system()` 里加 label / bar / 数字显示
5. 1 Hz 的 `lv_timer_create(tick_cb, 1000, NULL)` 会自动刷新

### Q8: 我想接光敏自动调背光?

预留了 `lib/LightSensor/`(GPIO1 ADC),但目前**没有接入主流程**。
可以这样做:

1. 在 `main.cpp` 创建一个 200 ms 的 task 读 ADC
2. 把读到的值通过 `TFT_BL_SetBrightness(0.2 + 0.8 * normalized)` 平滑映射到 0.2–1.0

---

## 🧾 开源协议

本项目基于 **MIT License** 发布,你可以自由用于个人/商业项目,但请保留原作者署名。

```
MIT License

Copyright (c) 2026 <your-name>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the above copyright notice and this permission
notice appearing in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## 🙏 致谢

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) — 强大的 SPI/LVGL 显示驱动库
- [LVGL](https://lvgl.io/) — 嵌入式 UI 框架
- [Espressif ESP-IDF](https://github.com/espressif/esp-idf) — 官方 SDK
- [psutil](https://github.com/giampaolo/psutil) — Python 跨平台硬件监控库

如果这个项目对你有用,欢迎给个 ⭐ Star!