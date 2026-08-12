#ifndef WIFI_PROVISION_H
#define WIFI_PROVISION_H

#include <stdint.h>
#include <string.h>

// 从 NVS 读取保存的 ssid/pass；找到返回 1，否则返回 0
int WifiProvision_Load(char *ssid, size_t ssid_len,
                       char *pass, size_t pass_len);
// 从 NVS 读取保存的 ssid/pass；找到返回 1，否则返回 0
void WifiProvision_ClearCredentials(void);

// 启动 AP 模式 + HTTP 服务器（含 / /scan /save /generate_204 路由）
// + DNS 重定向（让手机自动弹登录页）。
// 该函数会一直阻塞，直到用户在网页上提交凭证后 esp_restart()。
void WifiProvision_StartAP(void);


#endif
