#ifndef NETSYNC_H
#define NETSYNC_H

#include <stdint.h>
#include <stddef.h>

// Snapshot of the latest PC stats received over UDP.
// Updated by the background recv task; read from any context.
typedef struct {
    uint8_t   valid;             // 1 after first packet received
    uint8_t   cpu;               // 0..100
    uint8_t   cores[8];          // per-core load 0..100
    uint16_t  freq_mhz;
    uint8_t   cpu_temp;          // celsius
    uint8_t   ram_pct;           // 0..100
    float     ram_used_gb;
    float     ram_total_gb;
    uint8_t   swap_pct;          // 0..100
    uint32_t  up_bps;            // bytes per second
    uint32_t  down_bps;          // bytes per second
    uint64_t  up_total;          // bytes
    uint64_t  down_total;        // bytes
    uint8_t   gpu_load;          // 0..100
    uint8_t   gpu_mem_pct;       // 0..100
    uint8_t   gpu_temp;          // celsius
    uint16_t  disk_read_kbs;
    uint16_t  disk_write_kbs;
    char      nic_name[16];
    uint32_t  uptime_sec;
} NetSync_Data;

// Initialise NVS, connect to the configured WiFi (hardcoded), and start
// the UDP JSON receiver that feeds NetSync_Get(). The function blocks
// until WiFi is up (or fails); a background task then keeps the UDP
// socket alive. Safe to call once from app_main.
void NetSync_StartBackground(void);

// Read-only accessor for the latest PC snapshot. Always non-NULL;
// check ->valid before using the fields.
const NetSync_Data *NetSync_Get(void);

#endif