#pragma once

#include <Arduino.h>
#include <stdint.h>

/** Meshtastic WiFi client stubs for Deck Pro WiFi/OTA menus. */

inline void initWifi() {}
inline void deinitWifi() {}
inline bool isWifiAvailable() { return true; }

inline void w20_wifi_suspend_meshtastic_reconnect() {}
inline void w20_wifi_resume_meshtastic_reconnect(bool = false) {}
inline void w20_wifi_abort_sta_reconnect() {}
inline void w20_wifi_suspend_meshtastic_app_services() {}
inline void w20_wifi_resume_meshtastic_app_services() {}

inline uint8_t getWifiDisconnectReason() { return 0; }
inline void clearWifiDisconnectReason() {}

extern bool needReconnect;
extern unsigned long rebootAtMsec;
extern unsigned long shutdownAtMsec;
