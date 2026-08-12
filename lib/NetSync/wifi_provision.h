#ifndef WIFI_PROVISION_H
#define WIFI_PROVISION_H

#include <stdint.h>
#include <string.h>

// 从 NVS 读取保存的 ssid/pass；找到返回 1，否则返回 0
int WifiProvision_Load(
    char *ssid,
    size_t ssid_len,
    char *pass,
    size_t pass_len
);

// 清除保存的 Wi-Fi
void WifiProvision_ClearCredentials(void);

// 启动 AP 配网。
// 成功完成配网并切换到正常 STA 后返回 true。
// 配网失败 / 初始化失败返回 false。
bool WifiProvision_StartAP(void);

#endif
