#ifndef PROVISION_UI_H
#define PROVISION_UI_H

#include "lvgl.h"

void ProvisionUI_Start();

void ProvisionUI_Update();

void ProvisionUI_SetWifiName(
    const char *ssid
);

void ProvisionUI_ShowWaiting();

void ProvisionUI_ShowConnecting();

void ProvisionUI_ShowError(
    const char *msg
);

void ProvisionUI_ShowSuccess();

bool ProvisionUI_IsActive();

#endif