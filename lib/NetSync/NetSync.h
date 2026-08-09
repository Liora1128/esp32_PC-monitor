#ifndef NETSYNC_H
#define NETSYNC_H

// Initialise NVS, connect to the configured WiFi, sync NTP, and fetch
// weather - all in a single FreeRTOS task. The task deletes itself
// when finished. Safe to call once from app_main.
void NetSync_StartBackground(void);

#endif