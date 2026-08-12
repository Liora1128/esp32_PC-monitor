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

    // 正在连接已经保存的 Wi-Fi
    NETSYNC_STATE_CONNECTING,

    // 正在进入配网
    NETSYNC_STATE_PROVISIONING,

    // 已经建立正常 Wi-Fi
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

} NetSync_Data;

// ============================================================
// 网络系统
// ============================================================

void NetSync_StartBackground(void);

// ============================================================
// 网络状态
// ============================================================

NetSync_State NetSync_GetState(void);

// ============================================================
// 获取 PC 数据
// ============================================================

const NetSync_Data *NetSync_Get(void);

#endif