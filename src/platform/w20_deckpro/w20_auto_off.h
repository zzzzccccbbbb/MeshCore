/**
 * 无操作定时关机：默认开启，固定 10 分钟 → SCREEN9 真关机（Reset 可起）。
 * Setting「Timed Shutdown」可关；开关状态存 NVS。
 * 触控 / Boot / Mesh 按键 kick；充电中暂停计时。
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
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

/** 有用户活动时调用（触控/Boot/Mesh 按键）。 */
void w20_auto_off_kick(void);

/** shell_loop 周期调用；开启且超时则触发一次关机页。 */
void w20_auto_off_poll(void);

#ifdef __cplusplus
}
#endif
