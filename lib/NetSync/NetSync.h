#ifndef NETSYNC_H
#define NETSYNC_H

#include <stdint.h>
#include <stddef.h>

// ============================================================
// 网络状态
// ============================================================

typedef enum
{
    NETSYNC_STATE_STARTING = 0,

    // 正在连接用户选择的 Wi-Fi
    NETSYNC_STATE_CONNECTING,

    // 等待手机配网
    NETSYNC_STATE_PROVISIONING,

    // 连接失败，等待用户重新选择 / 输入密码
    NETSYNC_STATE_PROVISION_ERROR,

    // 已保存 Wi-Fi，正常启动连接成功
    NETSYNC_STATE_CONNECT_SUCCESS,

    // 配网成功
    NETSYNC_STATE_PROVISION_SUCCESS,

    // 正常运行
    NETSYNC_STATE_READY

} NetSync_State;

// ============================================================
// PC 监控数据
// ============================================================

typedef struct
{
    uint8_t   valid;

    uint8_t   cpu;
    uint8_t   cores[8];

    uint16_t  freq_mhz;

    uint8_t   cpu_temp;

    // 本次数据包是否包含对应字段
    uint8_t   has_cpu_temp;

    uint8_t   ram_pct;

    float     ram_used_gb;
    float     ram_total_gb;

    uint8_t   swap_pct;

    uint32_t  up_bps;
    uint32_t  down_bps;

    uint64_t  up_total;
    uint64_t  down_total;

    uint8_t   gpu_load;
    uint8_t   gpu_mem_pct;
    uint8_t   gpu_temp;

    uint16_t  disk_read_kbs;
    uint16_t  disk_write_kbs;

    char      nic_name[16];

    uint32_t  uptime_sec;

    // 本次数据包是否包含 uptime
    uint8_t   has_uptime;

} NetSync_Data;

// ============================================================
// 网络系统
// ============================================================

void NetSync_StartBackground(void);

// ============================================================
// 网络状态
// ============================================================

NetSync_State NetSync_GetState(void);

void NetSync_SetState(
    NetSync_State state
);

// ============================================================
// 配网 UI 信息
//
// 注意：这里只保存数据，不直接操作 LVGL。
// ============================================================

// 设置当前正在尝试连接的 SSID
void NetSync_SetProvisionSSID(
    const char *ssid
);

// 获取当前正在尝试连接的 SSID
const char *NetSync_GetProvisionSSID(void);

// 设置配网失败提示
void NetSync_SetProvisionError(
    const char *msg
);

// 获取配网失败提示
const char *NetSync_GetProvisionError(void);

// ============================================================
// 获取 PC 数据
// ============================================================

const NetSync_Data *NetSync_Get(void);

#endif