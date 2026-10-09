/************************************************************************
 * FilePath     : ui_base.h
 * Author       : GX.Duan
 * LastEditors  : ShallowGreen123 2608653986@qq.com
 * Copyright (c): 2022 by GX.Duan, All Rights Reserved.
 * Github       : https://github.com/ShallowGreen123/lvgl_examples.git
 ************************************************************************/
#ifndef __UI_PORT_DECKPOR_H__
#define __UI_PORT_DECKPOR_H__

/*********************************************************************************
 *                                  INCLUDES
 * *******************************************************************************/
#include "lvgl.h"

#include "peripheral.h"
#include "ui_deckpro.h"
#include "utilities.h"
#include "ui_wifi_provision.h"

#ifdef __cplusplus
extern "C" {
#endif
/*********************************************************************************
 *                                   DEFINES
 * *******************************************************************************/
// default language
#define DEFAULT_LANGUAGE_EN 0 // English
#define DEFAULT_LANGUAGE_CN 1 // Chinese

/*********************************************************************************
 *                                   MACROS
 * *******************************************************************************/

/*********************************************************************************
 *                                  TYPEDEFS
 * *******************************************************************************/

/*********************************************************************************
 *                              GLOBAL PROTOTYPES
 * *******************************************************************************/
void ui_disp_full_refr(void);

// [ screen 1 ] --- lora
float ui_lora_get_freq(void);
int ui_lora_get_mode(void);
void ui_lora_set_mode(int mode);
void ui_lora_send(const char *str);
void ui_lora_recv_loop(void);
const char *ui_lora_get_last_sent(void);
bool ui_lora_take_tx(const char **out);
bool ui_lora_get_recv(const char **str, int *rssi, float *snr);
void ui_lora_set_recv_flag(void);

// [ screen 2 ] --- setting
void ui_setting_set_language(int language);
void ui_setting_set_keypad_light(bool on);
void ui_setting_set_led_status(bool on);
/** 仅更新照明开关内存与 Setting 页标签（不改 GPIO / lamp_mode，用于闪烁等）。 */
void ui_setting_set_led_display_state(bool on);
/** 仅驱动照明 GPIO（用于按键闪烁动画；不触发 peri_lighting_led_key 状态同步）。 */
void ui_lighting_lamp_hw_only(bool on);
void ui_setting_set_gps_status(bool on);
void ui_setting_set_lora_status(bool on);
void ui_setting_set_rgb_mode(int mode);
int ui_setting_get_rgb_mode(void);
void ui_setting_set_a7682_status(bool on);

int ui_setting_get_language(void);
bool ui_setting_get_keypad_light(void);
bool ui_setting_get_led_status(void);
bool ui_setting_get_gps_status(void);  
bool ui_setting_get_lora_status(void); 
bool ui_setting_get_a7682_status(void);

// setting - > About System
const char *ui_setting_get_sf_ver(void);
const char *ui_setting_get_hd_ver(void);

// Optional: IP5356 helpers for battery screen (enabled when USE_IP5356_CHARGER=1)
bool ui_ip5356_present();
int  ui_ip5356_batt_mv();
bool ui_ip5356_status(bool &inputPresent, bool &charging, bool &discharging, bool &fault);
/** CHG_STATE2 bit7：C 口输入快充已建立（DPDM/PD），见 IP5356M 寄存器手册 */
bool ui_ip5356_sink_fast_charge_ok(void);

// [ screen 3 ] --- GPS
void ui_gps_task_suspend(void);
void ui_gps_task_resume(void);
void ui_gps_get_coord(double *lat, double *lng);
void ui_gps_get_data(uint16_t *year, uint8_t *month, uint8_t *day);
void ui_gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second);
/** Meshtastic 本地时钟（时区已应用）；无有效 RTC 时返回 false。 */
bool ui_clock_get_local_hm(uint8_t *hour, uint8_t *minute);
void ui_gps_get_satellites(uint32_t *vsat);
void ui_gps_get_speed(double *speed);
void ui_gps_get_altitude(double *alt);
void ui_gps_get_course(double *course);
void ui_gps_get_hdop(float *hdop);
bool ui_gps_get_fix_valid(void);

// [ screen 4 ] --- Wifi Scan
/** 按 SSID 去重（保留最强 RSSI），结果按信号强度降序。ap_count 为当前 WiFi 扫描缓冲条数。 */
int ui_wifi_dedup_scan_results(ui_wifi_scan_entry_t *out, int max_out, int ap_count, bool skip_open, bool skip_chinese);
void ui_wifi_get_scan_info(ui_wifi_scan_info_t *list, int list_len);
/** 异步 WiFi 扫描：start 后用 poll；返回 1=已启动/进行中，0=不可扫。 */
int ui_wifi_scan_start_async(void);
/** -1=扫描中，-2=失败，>=0=结果条数已写入 list。 */
int ui_wifi_scan_poll_info(ui_wifi_scan_info_t *list, int list_len);
/** 进入 WiFi 主菜单（SCREEN4）时打开 STA。 */
void ui_wifi_menu_power_on(void);
/** 离开 WiFi 主菜单栈：异步停 SoftAP/OTA，idle 后再断射频并交回 Meshtastic（不阻塞 LVGL）。 */
void ui_wifi_menu_power_off(void);
/** 进入 WiFi 主菜单：暂停 Meshtastic WiFi 重连（沙盒）。 */
void ui_wifi_menu_session_begin(void);
/** 离开 WiFi 主菜单：恢复 Meshtastic WiFi 重连。 */
void ui_wifi_menu_session_end(bool sta_connected_with_ip);
/** True while SCREEN4 WiFi menu stack is active. */
bool ui_wifi_menu_session_active(void);
void ui_wifi_connect(const char *ssid, const char *password);
void ui_wifi_disconnect(void);
/** Drop STA only; keep radio up for SoftAP portal. */
void ui_wifi_disconnect_sta(void);
/** SoftAP 强制重配：硬断 STA 并禁止 Meshtastic 自动重连。 */
void ui_wifi_force_drop_sta_for_softap(void);
bool ui_wifi_is_connected(void);
void ui_wifi_get_ip(char *buf, int len);
/** 已连上路由器时返回当前 STA SSID，否则返回空串。 */
void ui_wifi_get_sta_ssid(char *buf, int len);
/** 已保存的路由器 SSID（来自 config），与是否已连接无关。 */
void ui_wifi_get_saved_ssid(char *buf, int len);
/** STA 已关联时返回 RSSI (dBm)，未连接返回 0。 */
int ui_wifi_get_sta_rssi(void);
/** 异步 ping 网关更新丢包率；WiFi Config 等界面周期调用。 */
void ui_wifi_poll_sta_packet_loss(void);
/** 累计 ping 网关统计（TX/RX 到 1000 归零）；loss_pct/tx/rx 为 -1 表示尚无有效数据。 */
typedef struct {
    int loss_pct;
    int tx;
    int rx;
} ui_wifi_ping_stats_t;
/** 丢包率 0–100 (%)；未连接、无网关或尚未完成探测时返回 -1。 */
int ui_wifi_get_sta_packet_loss(void);
/** 读取 ping 统计（与 ui_wifi_get_sta_packet_loss 同源）。 */
void ui_wifi_get_sta_ping_stats(ui_wifi_ping_stats_t *stats);
/** 离开 WiFi Config 等界面时停止 ping 并重置缓存。 */
void ui_wifi_reset_sta_packet_loss(void);
/** STA 已连接且 localIP 非 0.0.0.0。 */
bool ui_wifi_sta_has_valid_ip(void);
/** 使用 config 中已保存凭据重连，timeout_ms 内等待有效 IP。 */
bool ui_wifi_reconnect_saved_sta(unsigned timeout_ms);
/** 进入 OTA 前：停 Config/OTA 服务，保留 STA，禁止全射频 OFF。 */
void ui_wifi_prepare_for_ota(void);

// [ screen 5 ] --- State
bool ui_test_get(int peri_id);
bool ui_test_a7682e(void);
bool ui_test_pcm5102(void);

// [ screen 6 ] --- Battery
/* IP5356 — PMIC + 简易 SOC（无 BQ27220 电流/温度/mAh） */
bool ui_battery_gauge_ok(void);
bool ui_battery_vbus_in(void);
bool ui_battery_charging(void);
bool ui_battery_discharging(void);
bool ui_battery_fault(void);
bool ui_battery_fast_charge_ok(void);
uint8_t ui_battery_chg_state(void);
bool ui_battery_charge_complete(void);
uint16_t ui_battery_status_raw(void);
uint8_t ui_battery_status_raw_d0(void);
uint16_t ui_battery_voltage_mv(void);
int16_t ui_battery_current_ma(void);
uint16_t ui_battery_ntc_mv(void);
/** 有效返回摄氏度；无效返回 -999。 */
int16_t ui_battery_ntc_temp_c(void);
uint16_t ui_battery_input_voltage_mv(void);
/** Battery 第一行：充电 Present+mV；放电且 Vbus ADC>0 时 Out+mV；否则 Absent。 */
void ui_battery_power_line(char *buf, int len);
uint16_t ui_battery_remain_mah(void);
uint16_t ui_battery_full_mah(void);
uint16_t ui_battery_percent(void);
/** true = SOC 来自电压线性估算，非 0x7B 寄存器 */
bool ui_battery_soc_is_estimated(void);
uint16_t ui_battery_health_percent(void);
const char *ui_battery_charge_state_label(void);
const char *ui_battery_lv_symbol(void);

/** GPIO10 / IP5356 KEY：IP6802 状态线（Battery 页显示 idle/low/sleep）。 */
void ui_battery_key_line(char *buf, int len);

// [ screen 7 ] --- Input
int ui_input_get_touch_coord(int *x, int *y);
int ui_input_get_keypay_val(char *v);
void ui_input_set_keypay_flag(void);
// GPIO keys (BOOT / LED_KEY only; RST is EN)
int ui_input_read_boot_key();   // returns 1 when pressed
int ui_input_read_led_key();    // returns 1 when pressed
/** IP5356 KEY @ GPIO10：INPUT，IP6802 拉低为有效；上电 init 一次。禁止 OUTPUT。 */
void ui_input_ip5356_key_init(void);
int ui_input_read_ip5356_key(void); // returns 1 when KEY low
/**
 * shell_loop：KEY 持续低→key_sleep；KEY 回高→清 sleep，并对开机未识别的 IP5356 限频重探。
 * 无短/长按；Battery/Test 读路径不 begin。
 */
void ui_input_ip5356_key_poll(void);
int ui_other_get_LTR(int *ch0, int *ch1, int *ps);
int ui_other_get_gyro(float *gyro_x, float *gyro_y, float *gyro_z);

// [ screen 8 ] --- A7682E
bool ui_a7682_at_cb(const char *at_cmd);
void ui_a7682_call(const char *number);
void ui_a7682_hang_up(void);
void ui_a7682_loop_resume(void);
void ui_a7682_loop_suspend(void);

// shutdown
void ui_shutdown_on(void);

// [ screen 10 ] --- A7682E
bool ui_pcm5102_cb(const char *at_cmd);void ui_pcm5102_stop(void);
void ui_pcm5102_stop(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif /* __UI_PORT_DECKPOR_H__ */
