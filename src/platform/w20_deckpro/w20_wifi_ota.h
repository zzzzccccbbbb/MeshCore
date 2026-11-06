#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** WiFi OTA 上传文件首字节魔数（'W'）；构建产物见 w20-firmware-ota.bin */
#define W20_WIFI_OTA_FILE_MAGIC 0x57

/** 理想空闲堆；低于此会多轮 reclaim，并尽量 inline 启动（省任务栈）。 */
#ifndef W20_WIFI_OTA_MIN_HEAP
#define W20_WIFI_OTA_MIN_HEAP 40000u
#endif
/** 硬门槛：配网后常态常只有 ~28–35KB。仅极端低堆才拒启。 */
#ifndef W20_WIFI_OTA_HARD_MIN_HEAP
#define W20_WIFI_OTA_HARD_MIN_HEAP 20000u
#endif

/** Wake deck sleep; require STA connected with IP (may reconnect saved WiFi). */
bool w20_wifi_ota_prepare_sta(void);

/** Alias for AdminModule OTA handoff (W20 uses browser OTA, not espota stub). */
bool w20_wifi_ota_prepare_admin_reboot(void);

/** Abort in-progress OTA start/reconnect (call when leaving OTA page). */
void w20_wifi_ota_abort_start(void);

/**
 * Start OTA HTTP :8080。
 * 已连路由器（有 STA IP）→ 局域网 STA；未联网 → SoftAP http://192.168.4.1:8080/ota。
 */
bool w20_wifi_ota_start(void);

/** Stop HTTP OTA；若为 SoftAP 模式则拆热点。 */
void w20_wifi_ota_stop(void);

/** Poll WebServer; call from shell_loop. */
void w20_wifi_ota_loop(void);

bool w20_wifi_ota_is_running(void);

/** True while firmware upload/write in progress. */
bool w20_wifi_ota_is_busy(void);

/** True while SoftAP OTA radio is up. */
bool w20_wifi_ota_is_ap_mode(void);

/** STA: http://LAN:8080/ota ；SoftAP: http://192.168.4.1:8080/ota */
void w20_wifi_ota_get_url(char *buf, size_t len);

const char *w20_wifi_ota_get_status_text(void);

/** 0-100 while uploading, -1 otherwise */
int w20_wifi_ota_get_progress_pct(void);

/** 预擦进度 0–100；未在擦除时为 -1（串口/网页；屏上擦写中不刷百分比）。 */
int w20_wifi_ota_get_erase_pct(void);

/**
 * 擦完后「Server ready」上墨窗口：shell 在 is_running 时仍允许这一次 EPD flush。
 */
bool w20_wifi_ota_epd_paint_allowed(void);

/**
 * UI 已把就绪页刷到墨水屏后调用（擦完放墨窗口内）。
 */
void w20_wifi_ota_ui_guide_painted(void);

#ifdef __cplusplus
}
#endif

#endif
