/**
 * 无操作：默认开启。
 *   5 分钟 → Deck 软睡眠（MeshCore LoRa/BLE 仍跑，Boot 可唤醒）
 *  10 分钟 → SCREEN9 真关机（Reset 可起；不因手机已连接豁免）
 * Setting「Timed Shutdown」可关；开关状态存 NVS。
 * 触控 / Boot / Mesh 按键 kick；唤醒时 kick；充电中（醒着时）暂停计时。
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef W20_AUTO_SLEEP_MINUTES
#define W20_AUTO_SLEEP_MINUTES 5
#endif

#ifndef W20_AUTO_OFF_MINUTES
#define W20_AUTO_OFF_MINUTES 10
#endif

/** 开机读 NVS；未写过则默认开启。 */
void w20_auto_off_init(void);

/** 定时关机是否开启（Setting Timed Shutdown）。 */
bool w20_auto_off_get_enabled(void);

/** 写入 NVS 并生效；开启时重置空闲计时。 */
void w20_auto_off_set_enabled(bool enabled);

/** 有用户活动时调用（触控/Boot/Mesh 按键/睡眠唤醒）。 */
void w20_auto_off_kick(void);

/**
 * shell_loop 周期调用（睡眠中也要调，才能走到 10 分钟关机）。
 * 开启且超时：先软睡，再真关机。
 */
void w20_auto_off_poll(void);

#ifdef __cplusplus
}
#endif
