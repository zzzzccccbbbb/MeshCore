#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** WiFi OTA 上传文件首字节魔数（'W'）；构建产物见 w20-firmware-ota.bin */
#define W20_WIFI_OTA_FILE_MAGIC 0x57

/** Wake deck sleep; require STA connected with IP (may reconnect saved WiFi). */
bool w20_wifi_ota_prepare_sta(void);

/** Alias for AdminModule OTA handoff (W20 uses browser OTA, not espota stub). */
bool w20_wifi_ota_prepare_admin_reboot(void);

/** Abort in-progress OTA start/reconnect (call when leaving OTA page). */
void w20_wifi_ota_abort_start(void);

/** Start HTTP OTA on STA :8080 only (no SoftAP fallback). */
bool w20_wifi_ota_start(void);

/** Stop HTTP OTA server. */
void w20_wifi_ota_stop(void);

/** Poll WebServer; call from shell_loop. */
void w20_wifi_ota_loop(void);

bool w20_wifi_ota_is_running(void);

/** True while firmware upload/write in progress. */
bool w20_wifi_ota_is_busy(void);

/** Always false — SoftAP OTA removed; kept for UI compatibility. */
bool w20_wifi_ota_is_ap_mode(void);

/** STA: http://LAN:8080/ota ; offline: "Need WiFi first" */
void w20_wifi_ota_get_url(char *buf, size_t len);

const char *w20_wifi_ota_get_status_text(void);

/** 0-100 while uploading, -1 otherwise */
int w20_wifi_ota_get_progress_pct(void);

#ifdef __cplusplus
}
#endif

#endif
