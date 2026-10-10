/**
 * W20 Deck 调试日志：一模块一开关（默认全关）。
 *
 * 信息/过程日志 → W20_<MOD>_LOG*；报错/失败仍用裸 Serial.printf，不要包进宏。
 *
 * build_flags 示例：
 *   -D W20_LOG_BAT=1
 *   -D W20_LOG_WIFI=1
 *   -D W20_LOG_UI=1
 *   -D W20_LOG_TOUCH=1
 */
#pragma once

/* ---------- 模块开关（0/1）---------- */

#ifndef W20_LOG_BAT
#define W20_LOG_BAT 0
#endif

#ifndef W20_LOG_WIFI
#define W20_LOG_WIFI 0
#endif

#ifndef W20_LOG_UI
#define W20_LOG_UI 0
#endif

/* 未单独指定时跟随 UI（触控/Setting 噪声大时可单独 -D=0/1） */
#ifndef W20_LOG_TOUCH
#define W20_LOG_TOUCH W20_LOG_UI
#endif
#ifndef W20_LOG_SETTING
#define W20_LOG_SETTING W20_LOG_UI
#endif

#ifndef W20_LOG_GPS
#define W20_LOG_GPS 0
#endif
#ifndef W20_LOG_LORA
#define W20_LOG_LORA 0
#endif
#ifndef W20_LOG_LED_KEY
#define W20_LOG_LED_KEY 0
#endif
#ifndef W20_LOG_SOS
#define W20_LOG_SOS 0
#endif

/* ---------- 打印宏 ---------- */

#if W20_LOG_UI
#include "configuration.h"
#define W20_UI_LOG(...) LOG_INFO("[W20UI] " __VA_ARGS__)
#define W20_UI_SCR_LOG(...)                                                                                            \
    do {                                                                                                               \
        Serial.printf("[W20UI] ");                                                                                     \
        Serial.printf(__VA_ARGS__);                                                                                    \
        Serial.printf("\n");                                                                                           \
    } while (0)
#else
#define W20_UI_LOG(...) ((void)0)
#define W20_UI_SCR_LOG(...) ((void)0)
#endif

#if W20_LOG_BAT
#define W20_BAT_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_BAT_LOGLN(s) Serial.println(s)
#else
#define W20_BAT_LOG(...) ((void)0)
#define W20_BAT_LOGLN(s) ((void)0)
#endif

#if W20_LOG_WIFI
#define W20_WIFI_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_WIFI_LOGLN(s) Serial.println(s)
#else
#define W20_WIFI_LOG(...) ((void)0)
#define W20_WIFI_LOGLN(s) ((void)0)
#endif

#if W20_LOG_SOS
#define W20_SOS_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_SOS_LOGLN(s) Serial.println(s)
#else
#define W20_SOS_LOG(...) ((void)0)
#define W20_SOS_LOGLN(s) ((void)0)
#endif

#if W20_LOG_LED_KEY
#define W20_LED_KEY_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_LED_KEY_LOGLN(s) Serial.println(s)
#else
#define W20_LED_KEY_LOG(...) ((void)0)
#define W20_LED_KEY_LOGLN(s) ((void)0)
#endif

#if W20_LOG_LORA
#define W20_LORA_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_LORA_LOGLN(s) Serial.println(s)
#else
#define W20_LORA_LOG(...) ((void)0)
#define W20_LORA_LOGLN(s) ((void)0)
#endif

#if W20_LOG_GPS
#define W20_GPS_LOG(...) Serial.printf(__VA_ARGS__)
#define W20_GPS_LOGLN(s) Serial.println(s)
#else
#define W20_GPS_LOG(...) ((void)0)
#define W20_GPS_LOGLN(s) ((void)0)
#endif

#if W20_LOG_TOUCH
#define W20_TOUCH_LOG(...) Serial.printf("[TOUCH] " __VA_ARGS__)
#define W20_TOUCH_LOGLN(s) Serial.println(s)
#else
#define W20_TOUCH_LOG(...) ((void)0)
#define W20_TOUCH_LOGLN(s) ((void)0)
#endif

#if W20_LOG_SETTING
#define W20_SETTING_LOG(...) Serial.printf("[Setting] " __VA_ARGS__)
#define W20_SETTING_LOGLN(s) Serial.println(s)
#else
#define W20_SETTING_LOG(...) ((void)0)
#define W20_SETTING_LOGLN(s) ((void)0)
#endif
