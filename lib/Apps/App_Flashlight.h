#ifndef APP_FLASHLIGHT_H
#define APP_FLASHLIGHT_H

#include "Menu_System.h"

extern "C" const menu_app_t App_Flashlight;

// Implemented in 1_3TFT - we declare here to keep this app self-contained.
void TFT_BL_SetBrightness(float brightness);

#endif