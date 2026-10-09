#pragma once

/**
 * ESP-IDF aborts if WiFi modem sleep is disabled while Bluetooth is enabled:
 *   "Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled"
 * MeshCore companion keeps Arduino BLE up — never use WIFI_PS_NONE in that case.
 */
#include <WiFi.h>
#include <esp_wifi.h>

static inline void w20_wifi_apply_ps_coex_safe(void)
{
#if defined(BLE_PIN_CODE)
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
#else
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif
}
