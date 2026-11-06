#pragma once

/**
 * ESP-IDF aborts if WiFi modem sleep is disabled while Bluetooth controller is on:
 *   "Should enable WiFi modem sleep when both WiFi and Bluetooth are enabled"
 *
 * WiFi 菜单/OTA 会 SerialBLEInterface::shutdownController()（deinit false）。
 * controller 关掉后可用 WIFI_PS_NONE；恢复 BLE 后必须再走 MIN_MODEM。
 */
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_bt.h>

static inline bool w20_bt_controller_on(void)
{
#if defined(CONFIG_BT_ENABLED) || defined(BLE_PIN_CODE)
    const esp_bt_controller_status_t st = esp_bt_controller_get_status();
    return st == ESP_BT_CONTROLLER_STATUS_ENABLED || st == ESP_BT_CONTROLLER_STATUS_INITED;
#else
    return false;
#endif
}

static inline void w20_wifi_apply_ps_coex_safe(void)
{
    if (w20_bt_controller_on()) {
        WiFi.setSleep(true);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    } else {
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
}

/**
 * OTA：只关 modem sleep（controller 已关则 PS_NONE）。
 * 勿在已关联 STA 上 set_bandwidth / setTxPower —— 配网路径已注明二者会
 * stop/start 射频，表现为 SoftAP 门户可达、OTA Server ready 后零 HTTP。
 */
static inline void w20_wifi_ota_rf_boost(void)
{
    w20_wifi_apply_ps_coex_safe();
}
