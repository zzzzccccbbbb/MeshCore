/**
 * Meshtastic configuration.h stub for MeshCore + W20 Deck Pro UI.
 * Provides LOG_* macros and a minimal `config` global.
 */
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

#ifndef MESHTASTIC_EXCLUDE_WEBSERVER
#define MESHTASTIC_EXCLUDE_WEBSERVER 1
#endif

#ifndef MESHTASTIC_EXCLUDE_GPS
#define MESHTASTIC_EXCLUDE_GPS 1
#endif

#ifndef MESHTASTIC_EXCLUDE_BLUETOOTH
#define MESHTASTIC_EXCLUDE_BLUETOOTH 1
#endif

#ifndef DISABLE_WELCOME_UNSET
#define DISABLE_WELCOME_UNSET 1
#endif

#ifndef HAS_GPS
#define HAS_GPS 0
#endif

#ifndef HAS_BLUETOOTH
#define HAS_BLUETOOTH 0
#endif

#ifndef ARCH_ESP32
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
#define ARCH_ESP32
#endif
#endif

#ifndef USE_EINK
#define USE_EINK
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(...)                                                                                                 \
    do {                                                                                                               \
        Serial.printf(__VA_ARGS__);                                                                                    \
        Serial.println();                                                                                              \
    } while (0)
#endif
#ifndef LOG_INFO
#define LOG_INFO(...)                                                                                                  \
    do {                                                                                                               \
        Serial.printf(__VA_ARGS__);                                                                                    \
        Serial.println();                                                                                              \
    } while (0)
#endif
#ifndef LOG_WARN
#define LOG_WARN(...)                                                                                                  \
    do {                                                                                                               \
        Serial.printf(__VA_ARGS__);                                                                                    \
        Serial.println();                                                                                              \
    } while (0)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(...)                                                                                                 \
    do {                                                                                                               \
        Serial.printf(__VA_ARGS__);                                                                                    \
        Serial.println();                                                                                              \
    } while (0)
#endif

enum meshtastic_Config_NetworkConfig_AddressMode {
    meshtastic_Config_NetworkConfig_AddressMode_DHCP = 0,
    meshtastic_Config_NetworkConfig_AddressMode_STATIC = 1,
};

enum meshtastic_Config_LoRaConfig_RegionCode {
    meshtastic_Config_LoRaConfig_RegionCode_UNSET = 0,
    meshtastic_Config_LoRaConfig_RegionCode_US = 1,
};

enum meshtastic_Config_PositionConfig_GpsMode {
    meshtastic_Config_PositionConfig_GpsMode_DISABLED = 0,
    meshtastic_Config_PositionConfig_GpsMode_ENABLED = 1,
    meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT = 2,
};

struct meshtastic_Config_NetworkConfig_IpV4Config {
    uint32_t ip;
    uint32_t gateway;
    uint32_t subnet;
    uint32_t dns;
};

struct meshtastic_Config_NetworkConfig {
    char wifi_ssid[33];
    char wifi_psk[65];
    bool wifi_enabled;
    meshtastic_Config_NetworkConfig_AddressMode address_mode;
    meshtastic_Config_NetworkConfig_IpV4Config ipv4_config;
};

struct meshtastic_Config_LoRaConfig {
    meshtastic_Config_LoRaConfig_RegionCode region;
};

struct meshtastic_Config_PositionConfig {
    meshtastic_Config_PositionConfig_GpsMode gps_mode;
};

struct meshtastic_Config_BluetoothConfig {
    bool enabled;
};

struct meshtastic_Config {
    meshtastic_Config_NetworkConfig network;
    meshtastic_Config_LoRaConfig lora;
    meshtastic_Config_PositionConfig position;
    meshtastic_Config_BluetoothConfig bluetooth;
};

extern meshtastic_Config config;
