/************************************************************************
 * W20 Deck Pro — WiFi captive-portal provisioning (SoftAP + HTTP + DNS)
 ************************************************************************/
#ifndef __UI_WIFI_PROVISION_H__
#define __UI_WIFI_PROVISION_H__

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_WIFI_PROV_IDLE = 0,
    UI_WIFI_PROV_STARTING,
    UI_WIFI_PROV_AP,
    UI_WIFI_PROV_CONNECTING,
    UI_WIFI_PROV_CONNECTED,
    UI_WIFI_PROV_FAILED,
} ui_wifi_prov_state_t;

/** True while SoftAP/portal 运行中（不含仅 STA 连接阶段）。 */
bool ui_wifi_provisioning_active(void);

/** Start SoftAP Mesh-XXXXXX, DNS hijack, and provisioning HTTP server. */
bool ui_wifi_provisioning_start(bool force_reconfig);

/** SoftAP @ 192.168.4.1 for OTA-only (no DNS / port 80). keep_sta: do not drop STA. ota_ap_only: pure WIFI_AP (no STA iface). */
bool ui_wifi_softap_begin(bool keep_sta, bool ota_ap_only);
void ui_wifi_softap_end(bool keep_sta);
bool ui_wifi_softap_is_up(void);
void ui_wifi_softap_get_ssid(char *buf, size_t len);

/** Pause NimBLE before bringing up WiFi (ESP32 coexistence). */
void ui_wifi_pause_ble_for_radio(void);

/** Stop portal services; disconnect STA unless already saved and user keeps WiFi menu open. */
void ui_wifi_provisioning_stop(void);

/** Non-blocking: ask provTask to stop SoftAP/portal (safe from LVGL/UI thread). */
void ui_wifi_provisioning_request_stop(void);

/** Wait until provisioning is idle (for OTA/background tasks). Returns true if idle. */
bool ui_wifi_provisioning_wait_stopped(unsigned timeout_ms);

/** Deauth AP clients, tear down SoftAP, normalize WiFi mode (keep_sta: retain STA if linked). */
void ui_wifi_force_ap_teardown(bool keep_sta);

/** Optional: LVGL 屏仅在状态/连接数变化时刷新（在配网任务上下文调用，内部应尽快返回）。 */
typedef void (*ui_wifi_prov_ui_cb_t)(void);
void ui_wifi_provisioning_set_ui_callback(ui_wifi_prov_ui_cb_t cb);

ui_wifi_prov_state_t ui_wifi_provisioning_get_state(void);

/** SoftAP 名（Mesh-XXXXXX）。未 start 过也会按 MAC 预填，供 Config 首帧墨水显示。 */
void ui_wifi_provisioning_get_ap_ssid(char *buf, size_t len);

/** 若尚未生成 AP SSID，按 MAC 预填（不起射频）。 */
void ui_wifi_provisioning_ensure_ap_ssid(void);

/** Human-readable status for device screen. */
void ui_wifi_provisioning_get_status_text(char *buf, size_t len);

int ui_wifi_provisioning_get_client_count(void);

/** 配网正在连路由器或已连上（由配网流程负责 Web 恢复，避免与 GOT_IP 重复 init）。 */
bool ui_wifi_provisioning_in_sta_handoff(void);

/**
 * SoftAP/门户结束后回收堆：清空 scan JSON、扫网结果、停门户 HTTP/DNS，
 * 并停掉空闲 wifiProv 任务（下次 Config start 再 ensureProvTask）。
 * OTA / 离开 Config 前应调用，勿在 SoftAP 运行中调用。
 */
void ui_wifi_provisioning_reclaim_heap(void);

/** 最近一次网页提交的路由器连接失败（密码/认证/超时等）。 */
bool ui_wifi_provisioning_connect_failed(void);

/** 配网流程中最近一次拿到的 STA IP（拆 SoftAP 瞬间 localIP 可能短暂为 0）。 */
bool ui_wifi_provisioning_get_last_sta_ip(char *buf, size_t len);

/**
 * 用户在 Config 点 Disconnect 后：保持断网，禁止自动 restore STA / Meshtastic 重连，
 * 直到配网成功。开机默认联网不受影响（标志默认 false）。
 */
void ui_wifi_user_hold_offline_set(bool hold);
bool ui_wifi_user_hold_offline(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_WIFI_PROVISION_H__ */
