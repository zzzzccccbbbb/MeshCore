#pragma once

#include "graphics/Screen.h"
#include <Arduino.h>
#include <stdint.h>
#include <string.h>

extern bool runASAP;
extern graphics::Screen *screen;

inline void getMacAddr(uint8_t *dmac)
{
    if (!dmac)
        return;
#if defined(ARDUINO_ARCH_ESP32)
    uint64_t mac = ESP.getEfuseMac();
    dmac[0] = (uint8_t)(mac >> 0);
    dmac[1] = (uint8_t)(mac >> 8);
    dmac[2] = (uint8_t)(mac >> 16);
    dmac[3] = (uint8_t)(mac >> 24);
    dmac[4] = (uint8_t)(mac >> 32);
    dmac[5] = (uint8_t)(mac >> 40);
#else
    memset(dmac, 0, 6);
#endif
}

inline void setBluetoothEnable(bool) {}
