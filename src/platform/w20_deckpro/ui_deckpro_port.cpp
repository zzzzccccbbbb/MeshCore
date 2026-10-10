
#include "Arduino.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "driver/gpio.h"
#endif
#include "ui_deckpro_port.h"
#include "factory.h"
#include "utilities.h"
#include "w20_dbg_log.h"
#include "peri_power.h"
#include "peri_lighting_led_key.h"

#include "FS.h"
#include "SPI.h"
#include <TinyGPS++.h>
#include "peripheral.h"
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "ui_deckpro.h"
#include "platform/w20_deckpro_shell/w20_lora_arbiter.h"
#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro_shell/w20_power_shutdown.h"
#include "platform/w20_deckpro/ui_wifi_provision.h"
#include "platform/w20_deckpro/w20_wifi_ota.h"
#include "platform/w20_deckpro/w20_wifi_ble_coex.h"
#if defined(BLE_PIN_CODE)
#include <helpers/esp32/SerialBLEInterface.h>
#endif
#include "mesh/NodeDB.h"
#include "mesh/wifi/WiFiAPClient.h"
/* WiFi stub APIs come from WiFiAPClient.h */
#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif
#endif
#include "WiFi.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#endif
#include "gps/RTC.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <TouchDrvCST3530.hpp>

static char s_ip5356_key_line[28] = "idle";
#if USE_IP5356_CHARGER
#include "w20_ip5356.h"
#if USE_CW2217_GAUGE
#include "w20_cw2217.h"
#endif
#include "w20_power_battery.h"
#include "power.h"
#include <limits.h>

#if USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356 && (PIN_VBAT_ADC >= 0)
#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-adc.h"
#endif

static void bat_adc_setup_once(void) {
  static bool done = false;
  if (done) return;
#if defined(ARDUINO_ARCH_ESP32)
  analogSetPinAttenuation((uint8_t)PIN_VBAT_ADC, ADC_11db);
#endif
  pinMode(PIN_VBAT_ADC, INPUT);
  done = true;
}

static int bat_adc_pin_mv(void) {
#if defined(ARDUINO_ARCH_ESP32)
  bat_adc_setup_once();
  return (int)analogReadMilliVolts(PIN_VBAT_ADC);
#else
  return 0;
#endif
}

static int bat_adc_vbat_mv(void) {
  long pin = bat_adc_pin_mv();
  return (int)((pin * (long)BAT_ADC_VBAT_NUM) / (long)BAT_ADC_VBAT_DEN);
}

static int battery_percent_from_mv(int mv) {
  if (mv <= IP5356_SOC_EMPTY_MV) return 0;
  if (mv >= IP5356_SOC_FULL_MV) return (int)W20_BAT_SOC_VOLTAGE_CAP_PCT; /* 100 只认 Finish */
  return (mv - IP5356_SOC_EMPTY_MV) * (int)W20_BAT_SOC_VOLTAGE_CAP_PCT /
         (IP5356_SOC_FULL_MV - IP5356_SOC_EMPTY_MV);
}
#endif /* USE_BAT_ADC_FALLBACK */
#endif /* USE_IP5356_CHARGER top */


// extern 
extern TouchDrvCST3530 touch;


volatile int default_language = DEFAULT_LANGUAGE_EN;
volatile bool default_keypad_light = false;
volatile bool default_led_status = false;
volatile bool default_gps_status = true;
volatile bool default_lora_status = true;
volatile int  default_rgb_mode = 0; // 0=OFF, 1=R+, 2=B+
volatile bool default_a7682_status = true;
// ----

void ui_disp_full_refr(void)
{
    disp_full_refr();
}
//************************************[ screen 0 ]****************************************** menu
//************************************[ screen 1 ]****************************************** lora

float ui_lora_get_freq(void)
{
    return LORA_FREQ;
}
int ui_lora_get_mode(void)
{
    return lora_get_mode();
}
void ui_lora_set_mode(int mode)
{
    lora_set_mode(mode);
}
void ui_lora_send(const char *str)
{
    lora_transmit(str);
}
void ui_lora_recv_loop(void)
{
    lora_receive_loop();
}
const char *ui_lora_get_last_sent(void)
{
    return lora_get_last_sent();
}
bool ui_lora_take_tx(const char **out)
{
    return lora_take_tx_for_ui(out);
}
bool ui_lora_get_recv(const char **str, int *rssi, float *snr)
{
    return lora_get_recv(str, rssi, snr);
}
void ui_lora_set_recv_flag(void)
{
    lora_set_recv_flag();
}
//************************************[ screen 2 ]****************************************** setting

#if 1
// set function
// DEFAULT_LANGUAGE_CN、DEFAULT_LANGUAGE_EN
void ui_setting_set_language(int language)
{
    default_language = language;
}
void ui_setting_set_keypad_light(bool on)
{
    if (BOARD_KEYBOARD_LED >= 0) digitalWrite(BOARD_KEYBOARD_LED, on);
    default_keypad_light = on;
}
void ui_lighting_lamp_hw_only(bool on)
{
    if (PIN_LIGHTING_LAMP < 0) {
        default_led_status = on;
        return;
    }
#if defined(ARDUINO_ARCH_ESP32)
    gpio_hold_dis((gpio_num_t)PIN_LIGHTING_LAMP);
#endif
    pinMode(PIN_LIGHTING_LAMP, OUTPUT);
#if defined(ARDUINO_ARCH_ESP32)
    gpio_set_drive_capability((gpio_num_t)PIN_LIGHTING_LAMP, GPIO_DRIVE_CAP_3);
#endif
    digitalWrite(PIN_LIGHTING_LAMP, on ? LIGHTING_LAMP_ON_LEVEL : LIGHTING_LAMP_OFF_LEVEL);
    default_led_status = on;
}

void ui_setting_set_led_display_state(bool on)
{
    default_led_status = on;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    ui_deckpro_setting_defer_refresh();
#endif
}

void ui_setting_set_led_status(bool on)
{
    ui_lighting_lamp_hw_only(on);
#if defined(ARDUINO_ARCH_ESP32) && W20_LOG_LED_KEY
    Serial.printf("[Lighting] on=%d gpio=%d read=%d\n", on ? 1 : 0, PIN_LIGHTING_LAMP,
                  digitalRead(PIN_LIGHTING_LAMP));
#endif
    peri_lighting_led_key_sync_from_ui(on);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    ui_deckpro_setting_defer_refresh();
#endif
}
void ui_setting_set_gps_status(bool on)
{
    // GPS3V3: HIGH=on, LOW=off
    if (BOARD_GPS_EN >= 0) {
        pinMode(BOARD_GPS_EN, OUTPUT);
        digitalWrite(BOARD_GPS_EN, on ? HIGH : LOW);
    }
    if (BOARD_GPS_WAKE >= 0) {
        pinMode(BOARD_GPS_WAKE, OUTPUT);
        digitalWrite(BOARD_GPS_WAKE, on ? HIGH : LOW);
    }
    default_gps_status = on;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (on) {
        const bool ok = w20_gps_mesh_reinit_after_module_power_on(300, 3000);
        if (!ok) {
            Serial.println("[GPS] power ON: Meshtastic GPS re-init FAILED");
        } else {
            W20_GPS_LOGLN("[GPS] power ON: Meshtastic GPS re-init OK");
        }
    } else {
        w20_gps_mesh_drop_on_module_power_off();
        W20_GPS_LOGLN("[GPS] power OFF: GPS drivers dropped");
    }
#endif
}
void ui_setting_set_lora_status(bool on)
{
    // LORA3V3: HIGH=on, LOW=off
    if (BOARD_LORA_EN >= 0) {
        pinMode(BOARD_LORA_EN, OUTPUT);
        digitalWrite(BOARD_LORA_EN, on ? HIGH : LOW);
    }
    default_lora_status = on;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (on) {
        const bool ok = w20_lora_mesh_reinit_after_module_power_on(80, 1500);
        if (!ok) {
            Serial.println("[LoRa] power ON: Meshtastic radio re-init FAILED");
        } else {
            W20_LORA_LOGLN("[LoRa] power ON: Meshtastic radio re-init OK");
        }
    } else {
        w20_lora_mesh_drop_on_module_power_off();
        W20_LORA_LOGLN("[LoRa] power OFF: radio drivers dropped");
    }
#endif
}
void ui_setting_set_rgb_mode(int mode)
{
    if (mode < 0) mode = 0;
    if (mode > 2) mode = 2;

    if (PIN_RGB_R >= 0) {
        pinMode(PIN_RGB_R, OUTPUT);
    }
    if (PIN_RGB_B >= 0) {
        pinMode(PIN_RGB_B, OUTPUT);
    }

    if (mode == 1) {
        // Common-cathode RGB: HIGH=on, LOW=off.
        if (PIN_RGB_R >= 0) digitalWrite(PIN_RGB_R, HIGH);
        if (PIN_RGB_B >= 0) digitalWrite(PIN_RGB_B, LOW);
    } else if (mode == 2) {
        if (PIN_RGB_R >= 0) digitalWrite(PIN_RGB_R, LOW);
        if (PIN_RGB_B >= 0) digitalWrite(PIN_RGB_B, HIGH);
    } else {
        if (PIN_RGB_R >= 0) digitalWrite(PIN_RGB_R, LOW);
        if (PIN_RGB_B >= 0) digitalWrite(PIN_RGB_B, LOW);
    }

    default_rgb_mode = mode;
}
void ui_setting_set_a7682_status(bool on)
{
    // enable 7682 module power
    if (BOARD_6609_EN >= 0) digitalWrite(BOARD_6609_EN, on);
    if (BOARD_A7682E_PWRKEY >= 0) digitalWrite(BOARD_A7682E_PWRKEY, on);
    default_a7682_status = on;
}

// get function
int ui_setting_get_language(void)
{
    return default_language;
}
bool ui_setting_get_keypad_light(void)
{
    return default_keypad_light;
}
bool ui_setting_get_led_status(void)
{
    return default_led_status;
}
bool ui_setting_get_gps_status(void)
{
    return default_gps_status;
}
bool ui_setting_get_lora_status(void)
{
    return default_lora_status;
}
int ui_setting_get_rgb_mode(void)
{
    return default_rgb_mode;
}
bool ui_setting_get_a7682_status(void)
{
    return default_a7682_status;
}

// About System
const char *ui_setting_get_sf_ver(void)
{
    return UI_T_DECK_PRO_VERSION;
}
const char *ui_setting_get_hd_ver(void)
{
    return BOARD_T_DECK_PRO_VERSION;
}

#endif
//************************************[ screen 3 ]****************************************** GPS
void ui_gps_task_suspend(void)
{
    gps_disable_for_screen();
}
void ui_gps_task_resume(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (!ui_setting_get_gps_status()) {
        return;
    }
#endif
    gps_enable_for_screen();
}
void ui_gps_get_coord(double *lat, double *lng)
{
    gps_get_coord(lat, lng);
}
void ui_gps_get_data(uint16_t *year, uint8_t *month, uint8_t *day)
{
    gps_get_data(year, month, day);
}
void ui_gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    gps_get_time(hour, minute, second);
}

bool ui_clock_get_local_hm(uint8_t *hour, uint8_t *minute)
{
    if (!hour || !minute) {
        return false;
    }
    const uint32_t rtc_sec = getValidTime(RTCQuality::RTCQualityDevice, true);
    if (!rtc_sec) {
        return false;
    }
    long hms = (rtc_sec % SEC_PER_DAY + SEC_PER_DAY) % SEC_PER_DAY;
    *hour = (uint8_t)(hms / SEC_PER_HOUR);
    *minute = (uint8_t)((hms % SEC_PER_HOUR) / SEC_PER_MIN);
    return true;
}

void ui_gps_get_satellites(uint32_t *vsat)
{
    gps_get_satellites(vsat);
}
void ui_gps_get_speed(double *speed)
{
    gps_get_speed(speed);
}
void ui_gps_get_altitude(double *alt)
{
    gps_get_altitude(alt);
}
void ui_gps_get_course(double *course)
{
    gps_get_course(course);
}
void ui_gps_get_hdop(float *hdop)
{
    gps_get_hdop(hdop);
}
bool ui_gps_get_fix_valid(void)
{
    return gps_get_fix_valid();
}
//************************************[ screen 4 ]****************************************** Wifi Scan
int is_chinese_utf8(const char *str) {
    unsigned char c = (unsigned char)str[0];
    return (c >= 0xE0 && c <= 0xEF);  // 检查第一个字节是否在 UTF-8 的中文字符范围内
}

static int wifi_scan_find_ssid(const ui_wifi_scan_entry_t *out, int count, const char *ssid)
{
    for (int j = 0; j < count; j++) {
        if (strcmp(out[j].ssid, ssid) == 0)
            return j;
    }
    return -1;
}

static void wifi_scan_sort_by_rssi_desc(ui_wifi_scan_entry_t *entries, int count)
{
    for (int i = 1; i < count; i++) {
        ui_wifi_scan_entry_t key = entries[i];
        int j = i - 1;
        while (j >= 0 && entries[j].rssi < key.rssi) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = key;
    }
}

int ui_wifi_dedup_scan_results(ui_wifi_scan_entry_t *out, int max_out, int ap_count, bool skip_open, bool skip_chinese)
{
    if (!out || max_out <= 0 || ap_count <= 0)
        return 0;

    int unique = 0;
    for (int i = 0; i < ap_count; i++) {
        if (skip_open && WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
            continue;

        String ssidStr = WiFi.SSID(i);
        const char *ssid = ssidStr.c_str();
        if (!ssid || ssid[0] == '\0')
            continue;
        if (skip_chinese && is_chinese_utf8(ssid))
            continue;

        const int rssi = WiFi.RSSI(i);
        const int idx = wifi_scan_find_ssid(out, unique, ssid);
        if (idx >= 0) {
            if (rssi > out[idx].rssi)
                out[idx].rssi = rssi;
            continue;
        }
        if (unique >= max_out)
            continue;

        strncpy(out[unique].ssid, ssid, sizeof(out[unique].ssid) - 1);
        out[unique].ssid[sizeof(out[unique].ssid) - 1] = '\0';
        out[unique].rssi = rssi;
        unique++;
    }

    if (unique > 1)
        wifi_scan_sort_by_rssi_desc(out, unique);
    return unique;
}

void ui_wifi_get_scan_info(ui_wifi_scan_info_t *list, int list_len)
{
    if (!list || list_len <= 0)
        return;

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    /* AP 模式下 WiFi.scan 会关掉 SoftAP，OTA 期间禁止扫描 */
    if (w20_wifi_ota_is_running() || w20_wifi_ota_is_busy() || w20_wifi_ota_is_ap_mode()) {
        memset(list, 0, sizeof(*list) * (size_t)list_len);
        return;
    }
#endif

    const int ap_count = WiFi.scanNetworks();
    ui_wifi_scan_entry_t entries[UI_WIFI_SCAN_ITEM_MAX];
    const int cap = (list_len > UI_WIFI_SCAN_ITEM_MAX) ? UI_WIFI_SCAN_ITEM_MAX : list_len;
    const int n = ui_wifi_dedup_scan_results(entries, cap, ap_count, false, true);

    memset(list, 0, sizeof(*list) * list_len);
    const int copy_n = n < list_len ? n : list_len;
    for (int i = 0; i < copy_n; i++) {
        strncpy(list[i].name, entries[i].ssid, sizeof(list[i].name) - 1);
        list[i].name[sizeof(list[i].name) - 1] = '\0';
        list[i].rssi = entries[i].rssi;
    }
}

int ui_wifi_scan_start_async(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (w20_wifi_ota_is_running() || w20_wifi_ota_is_busy() || w20_wifi_ota_is_ap_mode() ||
        ui_wifi_softap_is_up()) {
        return 0;
    }
#endif
    WiFi.scanDelete();
    /* 旧核 timeout = max_ms_per_chan*20；默认 300→6s 未扫完就 FAILED（与本次日志吻合） */
    const int ret = WiFi.scanNetworks(true /* async */, true /* show_hidden */, false, 1000);
    if (ret == WIFI_SCAN_FAILED) {
        Serial.println("[W20UI] WiFi async scan start FAILED");
        return 0;
    }
    W20_WIFI_LOGLN("[W20UI] WiFi async scan started");
    return 1;
}

int ui_wifi_scan_poll_info(ui_wifi_scan_info_t *list, int list_len)
{
    if (!list || list_len <= 0)
        return -2;

    const int n = WiFi.scanComplete();
    /* 只认明确的 RUNNING / FAILED；勿用 n<0 误伤（部分核心 RUNNING=-1） */
    if (n == WIFI_SCAN_RUNNING) {
        return -1;
    }
    if (n == WIFI_SCAN_FAILED) {
        /* 超时误报后驱动仍可能补 SCAN_DONE：短暂再等，避免空结果 */
        delay(200);
        const int n2 = WiFi.scanComplete();
        if (n2 == WIFI_SCAN_RUNNING) {
            return -1;
        }
        if (n2 >= 0) {
            W20_WIFI_LOG("[W20UI] WiFi scan recovered after FAILED → n=%d\n", n2);
            /* fall through with n2 */
            ui_wifi_scan_entry_t entries[UI_WIFI_SCAN_ITEM_MAX];
            const int cap = (list_len > UI_WIFI_SCAN_ITEM_MAX) ? UI_WIFI_SCAN_ITEM_MAX : list_len;
            const int unique = ui_wifi_dedup_scan_results(entries, cap, n2, false, true);
            WiFi.scanDelete();
            memset(list, 0, sizeof(*list) * (size_t)list_len);
            const int copy_n = unique < list_len ? unique : list_len;
            for (int i = 0; i < copy_n; i++) {
                strncpy(list[i].name, entries[i].ssid, sizeof(list[i].name) - 1);
                list[i].name[sizeof(list[i].name) - 1] = '\0';
                list[i].rssi = entries[i].rssi;
            }
            W20_WIFI_LOG("[W20UI] WiFi async scan done n=%d unique=%d\n", n2, copy_n);
            return copy_n;
        }
        WiFi.scanDelete();
        memset(list, 0, sizeof(*list) * (size_t)list_len);
        Serial.println("[W20UI] WiFi async scan failed");
        return -2;
    }
    if (n < 0) {
        return -1; /* 未知负值：当作仍在进行，勿 scanDelete */
    }

    ui_wifi_scan_entry_t entries[UI_WIFI_SCAN_ITEM_MAX];
    const int cap = (list_len > UI_WIFI_SCAN_ITEM_MAX) ? UI_WIFI_SCAN_ITEM_MAX : list_len;
    const int unique = ui_wifi_dedup_scan_results(entries, cap, n, false, true);
    WiFi.scanDelete();

    memset(list, 0, sizeof(*list) * (size_t)list_len);
    const int copy_n = unique < list_len ? unique : list_len;
    for (int i = 0; i < copy_n; i++) {
        strncpy(list[i].name, entries[i].ssid, sizeof(list[i].name) - 1);
        list[i].name[sizeof(list[i].name) - 1] = '\0';
        list[i].rssi = entries[i].rssi;
    }
    W20_WIFI_LOG("[W20UI] WiFi async scan done n=%d unique=%d\n", n, copy_n);
    return copy_n;
}

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro/ui_wifi_provision.h"

static bool s_wifiMenuSessionActive = false;

void ui_wifi_menu_session_begin(void)
{
    if (s_wifiMenuSessionActive) {
        return;
    }
    s_wifiMenuSessionActive = true;
    ui_wifi_pause_ble_for_radio();
    w20_wifi_suspend_meshtastic_reconnect();
    W20_WIFI_LOGLN("[W20UI] WiFi menu session begin (BLE paused / auto-reconnect suspended)");
}

void ui_wifi_menu_session_end(bool sta_connected_with_ip)
{
    if (!s_wifiMenuSessionActive) {
        return;
    }
    s_wifiMenuSessionActive = false;
    w20_wifi_resume_meshtastic_reconnect(sta_connected_with_ip);
#if defined(BLE_PIN_CODE)
    extern class SerialBLEInterface bluetooth_interface;
    if (!bluetooth_interface.isEnabled()) {
        bluetooth_interface.enable();
        W20_WIFI_LOGLN("[W20UI] MeshCore BLE re-enabled after WiFi menu");
    }
#endif
    W20_WIFI_LOGLN("[W20UI] WiFi menu session end");
}

bool ui_wifi_menu_session_active(void)
{
    return s_wifiMenuSessionActive;
}

bool ui_wifi_sta_has_valid_ip(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    char ip[24] = {0};
    ui_wifi_get_ip(ip, sizeof(ip));
    return ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0;
}

bool ui_wifi_reconnect_saved_sta(unsigned timeout_ms)
{
    if (ui_wifi_sta_has_valid_ip()) {
        return true;
    }
    if (ui_wifi_user_hold_offline()) {
        W20_WIFI_LOGLN("[W20UI] hold offline — skip STA reconnect");
        return false;
    }
    if (!config.network.wifi_ssid[0]) {
        return false;
    }
    ui_wifi_menu_power_on();
    const char *psk = config.network.wifi_psk[0] ? config.network.wifi_psk : NULL;
    ui_wifi_connect(config.network.wifi_ssid, psk);
    const unsigned long t0 = millis();
    while (millis() - t0 < timeout_ms) {
        if (ui_wifi_sta_has_valid_ip()) {
            W20_WIFI_LOG("[W20UI] STA reconnected %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        delay(100);
    }
    return ui_wifi_sta_has_valid_ip();
}

void ui_wifi_prepare_for_ota(void)
{
    W20_WIFI_LOGLN("[WiFiPrepOTA] begin");
    w20_wifi_ota_stop();
    /* 后台等配网真正停稳，禁止在 UI 线程 force teardown */
    if (!ui_wifi_provisioning_wait_stopped(3000)) {
        W20_WIFI_LOGLN("[WiFiPrepOTA] provisioning still active after wait");
    }
    if (ui_wifi_softap_is_up()) {
        W20_WIFI_LOGLN("[WiFiPrepOTA] tearing leftover SoftAP");
        ui_wifi_force_ap_teardown(ui_wifi_is_connected());
    }
    /* 配网收尾：释放 scan/门户/空闲任务堆，保证 OTA HTTP 能起来 */
    ui_wifi_provisioning_reclaim_heap();
    W20_WIFI_LOG("[WiFiPrepOTA] done sta=%d ip=%s heap=%u\n", ui_wifi_sta_has_valid_ip() ? 1 : 0,
                  WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
}
#else
void ui_wifi_menu_session_begin(void) {}
void ui_wifi_menu_session_end(bool sta_connected_with_ip) { (void)sta_connected_with_ip; }
bool ui_wifi_menu_session_active(void) { return false; }
bool ui_wifi_sta_has_valid_ip(void)
{
    return ui_wifi_is_connected() && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}
bool ui_wifi_reconnect_saved_sta(unsigned timeout_ms)
{
    (void)timeout_ms;
    return ui_wifi_sta_has_valid_ip();
}
void ui_wifi_prepare_for_ota(void) {}
#endif

void ui_wifi_menu_power_on(void)
{
    if (WiFi.getMode() == WIFI_OFF) {
        ui_wifi_pause_ble_for_radio();
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        delay(80);
        w20_wifi_apply_ps_coex_safe();
        W20_WIFI_LOGLN("[W20UI] WiFi radio on (STA)");
    }
}

void ui_wifi_menu_power_off(void)
{
    ui_wifi_provisioning_stop();
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_wifi_ota_stop();
#endif

    const bool keepSta =
        WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);

    if (keepSta) {
        W20_WIFI_LOGLN("[W20UI] WiFi menu exit: keep STA, Meshtastic web restart requested");
    } else {
        ui_wifi_disconnect();
        WiFi.mode(WIFI_OFF);
        W20_WIFI_LOGLN("[W20UI] WiFi menu exit: STA off");
    }

    /* 离开 WiFi 菜单：清配网残留，再交回 Meshtastic（含 Web） */
    ui_wifi_provisioning_reclaim_heap();
    ui_wifi_menu_session_end(keepSta);
}

void ui_wifi_connect(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password ? password : "");
}

void ui_wifi_disconnect(void)
{
    WiFi.disconnect(true, false);
}

/** Drop STA association but keep WiFi radio (for SoftAP reconfig). */
void ui_wifi_disconnect_sta(void)
{
    WiFi.disconnect(false, false);
}

/** SoftAP 强制配网前：硬断 STA + 禁止 Meshtastic 自动重连；若 SoftAP 已在则切纯 WIFI_AP。 */
void ui_wifi_force_drop_sta_for_softap(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_wifi_abort_sta_reconnect();
#endif
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    delay(50);
    const wifi_mode_t mode = WiFi.getMode();
    /* 已有 SoftAP：必须纯 AP，禁止 AP_STA 残留导致门户页一直刷新进不去 */
    if (mode == WIFI_AP_STA || mode == WIFI_AP) {
        WiFi.mode(WIFI_AP);
        delay(80);
        W20_WIFI_LOGLN("[W20UI] force SoftAP-only (drop STA iface)");
    } else if (mode == WIFI_STA) {
        WiFi.mode(WIFI_OFF);
        delay(50);
    }
}

bool ui_wifi_is_connected(void)
{
    return WiFi.status() == WL_CONNECTED;
}

void ui_wifi_get_ip(char *buf, int len)
{
    if (!buf || len <= 0) return;
    if (WiFi.status() != WL_CONNECTED) {
        lv_snprintf(buf, len, "0.0.0.0");
        return;
    }
    lv_snprintf(buf, len, "%s", WiFi.localIP().toString().c_str());
}

void ui_wifi_get_sta_ssid(char *buf, int len)
{
    if (!buf || len <= 0)
        return;
    buf[0] = '\0';
    if (WiFi.status() != WL_CONNECTED)
        return;
    String ssid = WiFi.SSID();
    if (ssid.length() > 0) {
        lv_snprintf(buf, len, "%s", ssid.c_str());
    }
}

void ui_wifi_get_saved_ssid(char *buf, int len)
{
    if (!buf || len <= 0)
        return;
    buf[0] = '\0';
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (config.network.wifi_ssid[0]) {
        lv_snprintf(buf, len, "%s", config.network.wifi_ssid);
    }
#endif
}

int ui_wifi_get_sta_rssi(void)
{
    if (WiFi.status() != WL_CONNECTED)
        return 0;
    return WiFi.RSSI();
}

#if defined(ARDUINO_ARCH_ESP32)
static esp_ping_handle_t s_wifiPingHdl = NULL;
static volatile int s_wifiPacketLossPct = -1;
static volatile int s_wifiPingTx = -1;
static volatile int s_wifiPingRx = -1;
static volatile bool s_wifiPingRunning = false;
static unsigned long s_wifiLastPingMs = 0;

static const uint32_t kWifiPingIntervalMs = 2000;
static const uint32_t kWifiPingCount = 1;
static const uint32_t kWifiPingIntervalStepMs = 200;
static const uint32_t kWifiPingTimeoutMs = 800;
static const int kWifiPingCumWrap = 1000;

static void ui_wifi_ping_update_cumulative(uint32_t transmitted, uint32_t received)
{
    if (transmitted == 0) {
        return;
    }
    if (s_wifiPingTx < 0) {
        s_wifiPingTx = 0;
        s_wifiPingRx = 0;
    }
    s_wifiPingTx += (int)transmitted;
    s_wifiPingRx += (int)received;
    if (s_wifiPingTx >= kWifiPingCumWrap) {
        s_wifiPingTx = 0;
        s_wifiPingRx = 0;
        s_wifiPacketLossPct = 0;
        return;
    }
    s_wifiPacketLossPct = (int)((1.0f - ((float)s_wifiPingRx / (float)s_wifiPingTx)) * 100.0f);
}

static void ui_wifi_ping_on_end(esp_ping_handle_t hdl, void *args)
{
    (void)args;
    uint32_t transmitted = 0;
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    ui_wifi_ping_update_cumulative(transmitted, received);
    esp_ping_delete_session(hdl);
    s_wifiPingHdl = NULL;
    s_wifiPingRunning = false;
}

static void ui_wifi_ping_stop(void)
{
    if (s_wifiPingHdl) {
        esp_ping_stop(s_wifiPingHdl);
        esp_ping_delete_session(s_wifiPingHdl);
        s_wifiPingHdl = NULL;
    }
    s_wifiPingRunning = false;
}
#endif

void ui_wifi_poll_sta_packet_loss(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    if (WiFi.status() != WL_CONNECTED) {
        ui_wifi_ping_stop();
        s_wifiPacketLossPct = -1;
        s_wifiPingTx = -1;
        s_wifiPingRx = -1;
        s_wifiLastPingMs = 0;
        return;
    }

    const IPAddress gateway = WiFi.gatewayIP();
    if (gateway == IPAddress(0, 0, 0, 0)) {
        s_wifiPacketLossPct = -1;
        s_wifiPingTx = -1;
        s_wifiPingRx = -1;
        return;
    }

    if (s_wifiPingRunning)
        return;

    const unsigned long now = millis();
    if (s_wifiLastPingMs != 0 && (now - s_wifiLastPingMs) < kWifiPingIntervalMs)
        return;

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    ip_addr_t target_addr;
    IP4_ADDR(ip_2_ip4(&target_addr), gateway[0], gateway[1], gateway[2], gateway[3]);
    IP_SET_TYPE_VAL(target_addr, IPADDR_TYPE_V4);
    config.target_addr = target_addr;
    config.count = kWifiPingCount;
    config.interval_ms = kWifiPingIntervalStepMs;
    config.timeout_ms = kWifiPingTimeoutMs;

    esp_ping_callbacks_t cbs = {};
    cbs.on_ping_end = ui_wifi_ping_on_end;

    if (esp_ping_new_session(&config, &cbs, &s_wifiPingHdl) != ESP_OK) {
        s_wifiPingHdl = NULL;
        return;
    }
    if (esp_ping_start(s_wifiPingHdl) != ESP_OK) {
        esp_ping_delete_session(s_wifiPingHdl);
        s_wifiPingHdl = NULL;
        return;
    }

    s_wifiPingRunning = true;
    s_wifiLastPingMs = now;
#else
    (void)0;
#endif
}

int ui_wifi_get_sta_packet_loss(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    return s_wifiPacketLossPct;
#else
    return -1;
#endif
}

void ui_wifi_get_sta_ping_stats(ui_wifi_ping_stats_t *stats)
{
    if (!stats) {
        return;
    }
#if defined(ARDUINO_ARCH_ESP32)
    stats->loss_pct = s_wifiPacketLossPct;
    stats->tx = s_wifiPingTx;
    stats->rx = s_wifiPingRx;
#else
    stats->loss_pct = -1;
    stats->tx = -1;
    stats->rx = -1;
#endif
}

void ui_wifi_reset_sta_packet_loss(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    ui_wifi_ping_stop();
    s_wifiPacketLossPct = -1;
    s_wifiPingTx = -1;
    s_wifiPingRx = -1;
    s_wifiLastPingMs = 0;
#endif
}
//************************************[ screen 5 ]****************************************** Test
bool ui_test_get(int peri_id)
{
#if USE_IP5356_CHARGER
    if (peri_id == E_PERI_IP5356) {
        /* 粘性 present + 最近读成败（与 BAT -1 / KEY hb ip= 同源）；不在此额外 begin/读 */
        return w20_ip5356_is_present() && w20_ip5356_last_read_ok();
    }
#endif
#if USE_CW2217_GAUGE
    if (peri_id == E_PERI_CW2217)
        return w20_cw2217_is_present();
#endif
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (peri_id == E_PERI_LORA)
        return w20_lora_hw_ok();
    if (peri_id == E_PERI_GPS)
        return w20_gps_hw_ok();
#endif
    return peri_init_st[peri_id];
}
bool ui_test_a7682e(void) 
{
    return peri_init_st[E_PERI_A7682E];
}
bool ui_test_pcm5102a(void)
{
    return peri_init_st[E_PERI_PCM5102A];
}

//************************************[ screen 6 ]****************************************** Battery
#if 1

#if USE_IP5356_CHARGER

bool ui_ip5356_present() {
  return w20_ip5356_is_present();
}

int ui_ip5356_batt_mv() {
  if (!w20_ip5356_is_present()) return -1;
  return w20_ip5356_read_voltage_mv();
}

bool ui_ip5356_status(bool &inputPresent, bool &charging, bool &discharging, bool &fault) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  inputPresent = st.inputPresent;
  charging = st.charging;
  discharging = st.boostEn;
  fault = st.vbusOv;
  return true;
}

bool ui_ip5356_sink_fast_charge_ok(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.sinkFastChargeOk;
}

bool ui_battery_gauge_ok(void) {
  if (w20_ip5356_is_present()) return true;
#if USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356 && (PIN_VBAT_ADC >= 0)
  return bat_adc_vbat_mv() >= 2500;
#endif
  return false;
}

bool ui_battery_vbus_in(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.inputPresent;
}

bool ui_battery_charging(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.charging;
}

bool ui_battery_discharging(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.boostEn;
}

bool ui_battery_fault(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.vbusOv;
}

bool ui_battery_fast_charge_ok(void) {
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.sinkFastChargeOk;
}

uint8_t ui_battery_chg_state(void) {
  if (!w20_ip5356_is_present()) return 0;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return 0;
  return st.chgState;
}

bool ui_battery_charge_complete(void) {
#if defined(W20_MESHNOLOGY)
  /* 对号：IP Finish 锁存 AND CW≥阈值；对号亮时 UI% 同步为 100 */
  return w20_battery_charge_complete_latched();
#else
  if (!w20_ip5356_is_present()) return false;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return false;
  return st.chgState == IP5356::kChgStateFull;
#endif
}

const char *ui_battery_charge_state_label(void) {
  if (!w20_ip5356_is_present()) return "Offline";
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return "Offline";
  if (st.vbusOv) return "VBUS OV";
  switch (st.chgState) {
  case IP5356::kChgStateTrickle: return "Trickle";
  case IP5356::kChgStateCc: return "CC";
  case IP5356::kChgStateCv: return "CV";
  case IP5356::kChgStateCvDetect: return "CV Det";
  case IP5356::kChgStateFull: return "Finish";
  case IP5356::kChgStateTimeout: return "Timeout";
  default: break;
  }
  if (st.charging) return "Charging";
  if (st.boostEn) return "Boost";
  if (st.inputPresent) return "Idle";
  return "Discharge";
}

uint16_t ui_battery_status_raw(void) {
  if (!w20_ip5356_is_present()) return 0;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return 0;
  return (uint16_t)((st.raw_c4 << 8) | st.raw_e9);
}

uint8_t ui_battery_status_raw_d0(void) {
  if (!w20_ip5356_is_present()) return 0;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return 0;
  return st.raw_d0;
}

uint16_t ui_battery_voltage_mv(void) {
  if (w20_ip5356_is_present()) {
    int mv = w20_ip5356_read_voltage_mv();
    if (mv > 0)
      return (uint16_t)mv;
  }
#if USE_CW2217_GAUGE
  if (w20_cw2217_is_present()) {
    int mv = w20_cw2217_read_voltage_mv();
    if (mv > 0)
      return (uint16_t)mv;
  }
#endif
#if USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356 && (PIN_VBAT_ADC >= 0)
  return (uint16_t)bat_adc_vbat_mv();
#else
  return 0;
#endif
}

uint16_t ui_battery_input_voltage_mv(void) {
  if (!w20_ip5356_is_present()) return 0;
  IP5356::Status st;
  if (!w20_ip5356_read_status(st)) return 0;
  if (st.vbusOk) {
    const int mv = w20_ip5356_read_vbus_mv();
    return (mv < 0) ? 0 : (uint16_t)mv;
  }
  if (st.vinOk) {
    const int mv = w20_ip5356_read_vin_mv();
    return (mv < 0) ? 0 : (uint16_t)mv;
  }
  return 0;
}

void ui_battery_power_line(char *buf, int len)
{
  if (!buf || len <= 0)
    return;
  if (!w20_ip5356_is_present()) {
    snprintf(buf, (size_t)len, "Absent");
    return;
  }
  if (ui_battery_vbus_in()) {
    const uint16_t inMv = ui_battery_input_voltage_mv();
    if (inMv > 0)
      snprintf(buf, (size_t)len, "Present %umV", (unsigned)inMv);
    else
      snprintf(buf, (size_t)len, "Present");
    return;
  }
  const int outMv = w20_ip5356_read_vbus_mv();
  if (outMv > 0)
    snprintf(buf, (size_t)len, "Out %umV", (unsigned)outMv);
  else
    snprintf(buf, (size_t)len, "Absent");
}

int16_t ui_battery_current_ma(void) {
  if (!w20_ip5356_is_present()) return 0;
  int ma = ui_battery_charging() ? w20_ip5356_read_input_current_ma() : w20_ip5356_read_bat_current_ma();
  if (ma < 0) return 0;
  if (ma > 32767) ma = 32767;
  return (int16_t)ma;
}

uint16_t ui_battery_ntc_mv(void) {
  if (!w20_ip5356_is_present()) return 0;
  const int mv = w20_ip5356_read_ntc_mv();
  return (mv < 0) ? 0 : (uint16_t)mv;
}

int16_t ui_battery_ntc_temp_c(void) {
  if (!w20_ip5356_is_present()) return -999;
  const int t = w20_ip5356_read_ntc_temp_c();
  if (t == INT16_MIN) return -999;
  if (t < -999) return -999;
  if (t > 999) return 999;
  return (int16_t)t;
}

uint16_t ui_battery_full_mah(void) {
  return (uint16_t)IP5356_BATTERY_DESIGN_MAH;
}

uint16_t ui_battery_remain_mah(void) {
  const uint32_t full = IP5356_BATTERY_DESIGN_MAH;
  const uint32_t pct = ui_battery_percent();
  return (uint16_t)((full * pct) / 100U);
}

uint16_t ui_battery_health_percent(void) {
#if USE_CW2217_GAUGE
  if (w20_cw2217_is_present()) {
    const int mv = w20_cw2217_read_voltage_mv();
    if (mv >= (int)W20_BAT_CW_VALID_MIN_MV) {
      const int soh = w20_cw2217_read_soh_percent();
      if (soh > 0)
        return (uint16_t)soh;
    }
  }
#endif
  return 100;
}

bool ui_battery_soc_is_estimated(void) {
  /* IP 主源试跑：0x7B 有效则非 est.；CW 主源：VCELL 有效则非 est.；否则 LUT → est. */
#if defined(W20_MESHNOLOGY) && W20_BAT_UI_SRC_IP5356
  if (w20_ip5356_is_present() && w20_ip5356_read_percent() >= 0)
    return false;
#endif
#if USE_CW2217_GAUGE
  if (w20_cw2217_is_present()) {
    const int mv = w20_cw2217_read_voltage_mv();
    if (mv >= (int)W20_BAT_CW_VALID_MIN_MV)
      return false;
  }
#endif
  return true;
}

uint16_t ui_battery_percent(void) {
#if defined(W20_MESHNOLOGY)
  const int p = w20_battery_read_percent();
  if (p >= 0)
    return (uint16_t)p;
#else
  if (w20_ip5356_is_present()) {
    int p = w20_ip5356_read_percent();
    if (p >= 0) {
      if (p > 100) p = 100;
      return (uint16_t)p;
    }
  }
#if USE_CW2217_GAUGE
  if (w20_cw2217_is_present()) {
    int p = w20_cw2217_read_percent();
    if (p < 0) return 0;
    if (p > 100) p = 100;
    return (uint16_t)p;
  }
#endif
#endif
#if USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356 && (PIN_VBAT_ADC >= 0)
  {
    int adcP = battery_percent_from_mv(bat_adc_vbat_mv());
    if (adcP < 0) return 0;
    if (adcP > 100) adcP = 100;
    return (uint16_t)adcP;
  }
#else
  return 0;
#endif
}

const char *ui_battery_lv_symbol(void) {
  int percent = (int)ui_battery_percent();
  /* 满格与数字 100 / 对号对齐；对号已要求 CW 近满，不会因 IP Finish 单独满格 */
  const bool full_icon = (percent >= 100) || ui_battery_charge_complete();
  if (full_icon)
    return LV_SYMBOL_BATTERY_FULL;
  if (percent < 10)
    return LV_SYMBOL_BATTERY_EMPTY; /* 0–9：真正低电 */
  if (percent < 30)
    return LV_SYMBOL_BATTERY_1; /* 10–29 ≈ 1/5 */
  if (percent < 50)
    return LV_SYMBOL_BATTERY_2; /* 30–49 ≈ 2/5 */
  if (percent < 70)
    return LV_SYMBOL_BATTERY_3; /* 50–69 ≈ 3/5 */
  return LV_SYMBOL_BATTERY_3;     /* 70–99：LVGL 无 4 格符号，用 3 格 */
}

void ui_battery_key_line(char *buf, int len)
{
    if (!buf || len <= 0)
        return;
    snprintf(buf, (size_t)len, "%s", s_ip5356_key_line);
}

#else

bool ui_ip5356_present() { return false; }
int ui_ip5356_batt_mv() { return -1; }
bool ui_ip5356_status(bool &, bool &, bool &, bool &) { return false; }
bool ui_ip5356_sink_fast_charge_ok(void) { return false; }

bool ui_battery_gauge_ok(void) { return false; }
bool ui_battery_vbus_in(void) { return false; }
bool ui_battery_charging(void) { return false; }
bool ui_battery_discharging(void) { return false; }
bool ui_battery_fault(void) { return false; }
bool ui_battery_fast_charge_ok(void) { return false; }
uint8_t ui_battery_chg_state(void) { return 0; }
bool ui_battery_charge_complete(void) { return false; }
uint16_t ui_battery_status_raw(void) { return 0; }
uint8_t ui_battery_status_raw_d0(void) { return 0; }
uint16_t ui_battery_voltage_mv(void) { return 0; }
int16_t ui_battery_current_ma(void) { return 0; }
uint16_t ui_battery_ntc_mv(void) { return 0; }
int16_t ui_battery_ntc_temp_c(void) { return -999; }
uint16_t ui_battery_input_voltage_mv(void) { return 0; }
void ui_battery_power_line(char *buf, int len)
{
  if (buf && len > 0)
    snprintf(buf, (size_t)len, "Absent");
}
uint16_t ui_battery_remain_mah(void) { return 0; }
uint16_t ui_battery_full_mah(void) { return 0; }
uint16_t ui_battery_percent(void) { return 0; }
bool ui_battery_soc_is_estimated(void) { return false; }
uint16_t ui_battery_health_percent(void) { return 0; }
const char *ui_battery_charge_state_label(void) { return "Offline"; }
const char *ui_battery_lv_symbol(void) { return LV_SYMBOL_BATTERY_EMPTY; }
void ui_battery_key_line(char *buf, int len)
{
  if (buf && len > 0)
    snprintf(buf, (size_t)len, "%s", s_ip5356_key_line);
}

#endif
#endif /* #if 1 screen 6 battery port */
//************************************[ screen 7 ]****************************************** Input
int ui_input_get_touch_coord(int *x, int *y)
{
    int16_t last_x = 0;
    int16_t last_y = 0;
    int ret = touch.getPoint(&last_x, &last_y);
    *x = last_x;
    *y = last_y;
    return ret;
}

int ui_input_get_keypay_val(char *v)
{
    #if USE_TCA8418_KEYPAD
    return keypad_get_val(v);
    #else
    if (v) *v = 0;
    return 0;
    #endif
}

void ui_input_set_keypay_flag(void)
{
    #if USE_TCA8418_KEYPAD
    keypad_set_flag();
    #endif
}

int ui_input_read_boot_key()
{
    if (PIN_KEY_BOOT < 0) return 0;
    /* 板载 3.3V 上拉，勿开内部上拉 */
    pinMode(PIN_KEY_BOOT, INPUT);
    return digitalRead(PIN_KEY_BOOT) == LOW ? 1 : 0;
}

int ui_input_read_led_key()
{
    if (PIN_KEY_LED < 0) return 0;
    return digitalRead(PIN_KEY_LED) == LOW ? 1 : 0;
}

static int s_ip5356_key_stable = 0;
static int s_ip5356_key_raw = 0;
static uint32_t s_ip5356_key_raw_ms = 0;
static uint32_t s_ip5356_key_low_ms = 0;
static bool s_ip5356_key_raw_init = false;
static uint32_t s_ip5356_key_hb_ms = 0;
static bool s_ip5356_key_force_cw_latched = false;
/** 无线充时 KEY 易抖动：稳定边沿最短间隔，抑制 sleep/reprobe 风暴 */
#ifndef W20_IP5356_KEY_EDGE_MIN_MS
#define W20_IP5356_KEY_EDGE_MIN_MS 800u
#endif
static uint32_t s_ip5356_key_edge_ms = 0;
#if USE_IP5356_CHARGER
static uint32_t s_ip5356_periodic_reprobe_ms = 0;

/** GPIO10 触发的限频重探；成功则刷新 peri/电源。读电量路径不调用。 */
static void ui_ip5356_runtime_reprobe_from_key(const char *reason)
{
    if (w20_ip5356_is_present())
        return;
#if defined(USE_W20_DECKPRO_SHELL)
    if (ui_deckpro_epd_flush_busy()) {
        W20_BAT_LOG("[KEY] skip IP reprobe (%s): epd_flush busy\n", reason ? reason : "?");
        return;
    }
#endif
    if (!w20_ip5356_try_runtime_reprobe(reason))
        return;
    if (!w20_ip5356_is_present())
        return;
    peri_init_st[E_PERI_IP5356] = true;
    w20_power_battery_on_ip5356_ready();
    W20_BAT_LOG("[KEY] IP5356 reprobe OK (%s)\n", reason ? reason : "?");
}
#endif

void ui_input_ip5356_key_init(void)
{
#if (PIN_IP5356_KEY >= 0)
    pinMode(PIN_IP5356_KEY, INPUT);
    s_ip5356_key_hb_ms = millis();
    s_ip5356_key_force_cw_latched = false;
    w20_battery_set_key_sleep_mode(false);
    W20_BAT_LOG("[KEY] GPIO%d init level=%d\n", PIN_IP5356_KEY, digitalRead(PIN_IP5356_KEY));
#endif
    strncpy(s_ip5356_key_line, "idle", sizeof(s_ip5356_key_line) - 1);
    s_ip5356_key_line[sizeof(s_ip5356_key_line) - 1] = '\0';
    s_ip5356_key_stable = 0;
    s_ip5356_key_raw_init = false;
    s_ip5356_key_edge_ms = 0;
#if USE_IP5356_CHARGER
    s_ip5356_periodic_reprobe_ms = 0;
#endif
}

int ui_input_read_ip5356_key(void)
{
#if (PIN_IP5356_KEY >= 0)
    return digitalRead(PIN_IP5356_KEY) == IP5356_KEY_ACTIVE_LEVEL ? 1 : 0;
#else
    return 0;
#endif
}

void ui_input_ip5356_key_poll(void)
{
#if (PIN_IP5356_KEY < 0)
    return;
#else
    /*
     * GPIO10 不是用户按键手势，而是 IP6802 状态线：
     * - 拔外部电后可能先有短暂低脉冲（约 2s 又拉高）→ 忽略
     * - 约 30s 后持续拉低 → 置 key_sleep（IP 可能休眠）；SOC 仍优先 IP，失败才电压 LUT
     * - 外部电恢复、KEY 回高 → 清 key_sleep，并对开机未识别到的 IP5356 做限频重探
     * - 不做短按/长按；Battery/Test 读路径仍禁止 begin
     */
    const uint32_t now = millis();
    const int gpio_lvl = digitalRead(PIN_IP5356_KEY);
    const int raw = (gpio_lvl == IP5356_KEY_ACTIVE_LEVEL) ? 1 : 0; /* 1=低=IP6802 拉低 */

#if W20_LOG_BAT
    if (s_ip5356_key_hb_ms == 0 || (uint32_t)(now - s_ip5356_key_hb_ms) >= W20_IP5356_KEY_HB_MS) {
        s_ip5356_key_hb_ms = now;
        W20_BAT_LOG("[KEY] GPIO%d hb level=%d low=%d key_sleep=%d bat=%s ip=%d\n", PIN_IP5356_KEY, gpio_lvl, raw,
                    w20_battery_key_sleep_mode() ? 1 : 0, w20_battery_ui_src_name(),
#if USE_IP5356_CHARGER
                    w20_ip5356_last_read_ok() ? 1 : 0
#else
                    0
#endif
        );
    }
#else
    (void)s_ip5356_key_hb_ms;
#endif

    if (!s_ip5356_key_raw_init) {
        s_ip5356_key_raw_init = true;
        s_ip5356_key_raw = raw;
        s_ip5356_key_stable = raw;
        s_ip5356_key_raw_ms = now;
        if (raw)
            s_ip5356_key_low_ms = now;
#if USE_IP5356_CHARGER
        /* 开机 KEY 已高且 IP 未就绪：无上升沿，安排较短周期后首次重探 */
        if (!raw && !w20_ip5356_is_present()) {
            s_ip5356_periodic_reprobe_ms =
                now - (W20_IP5356_REPROBE_PERIODIC_MS > W20_IP5356_REPROBE_MIN_MS
                           ? (W20_IP5356_REPROBE_PERIODIC_MS - W20_IP5356_REPROBE_MIN_MS)
                           : 0);
        }
#endif
        return;
    }
    if (raw != s_ip5356_key_raw) {
        s_ip5356_key_raw = raw;
        s_ip5356_key_raw_ms = now;
        return;
    }
    if ((uint32_t)(now - s_ip5356_key_raw_ms) < 20)
        return;

    if (raw == s_ip5356_key_stable) {
        /* 持续为低：满 FORCE_CW_MS 后置 key_sleep（滤掉 IP6802 短脉冲） */
        if (raw && !s_ip5356_key_force_cw_latched &&
            (uint32_t)(now - s_ip5356_key_low_ms) >= W20_IP5356_KEY_FORCE_CW_MS) {
            s_ip5356_key_force_cw_latched = true;
            w20_battery_set_key_sleep_mode(true);
            strncpy(s_ip5356_key_line, "sleep", sizeof(s_ip5356_key_line) - 1);
            W20_BAT_LOG("[KEY] sustained low %lums -> key_sleep\n", (unsigned long)(now - s_ip5356_key_low_ms));
        }
#if USE_IP5356_CHARGER
        /* KEY 保持高且仍未识别：慢速兜底重探（充电/后上电） */
        if (!raw && !w20_ip5356_is_present()) {
            if (s_ip5356_periodic_reprobe_ms == 0)
                s_ip5356_periodic_reprobe_ms = now;
            else if ((uint32_t)(now - s_ip5356_periodic_reprobe_ms) >= W20_IP5356_REPROBE_PERIODIC_MS) {
                s_ip5356_periodic_reprobe_ms = now;
                ui_ip5356_runtime_reprobe_from_key("key_high_periodic");
            }
        } else if (w20_ip5356_is_present()) {
            s_ip5356_periodic_reprobe_ms = 0;
        }
#endif
        return;
    }

    /* 无线充 KEY 抖：未满边沿间隔则丢弃本次稳定翻转（raw 状态保留，下次再判） */
    if (s_ip5356_key_edge_ms != 0 && (uint32_t)(now - s_ip5356_key_edge_ms) < W20_IP5356_KEY_EDGE_MIN_MS) {
        s_ip5356_key_raw_ms = now; /* 继续要求稳定，避免抖动窗口内反复翻转 */
        return;
    }
    s_ip5356_key_edge_ms = now;

    if (raw) {
        s_ip5356_key_stable = 1;
        s_ip5356_key_low_ms = now;
        s_ip5356_key_force_cw_latched = false;
        strncpy(s_ip5356_key_line, "low", sizeof(s_ip5356_key_line) - 1);
        W20_BAT_LOGLN(F("[KEY] low (start hold timer)"));
        return;
    }

    /* KEY 回高：外部供电恢复 / 脉冲结束 → 清 key_sleep，并尝试唤醒后的 I2C 重探 */
    s_ip5356_key_stable = 0;
    s_ip5356_key_force_cw_latched = false;
    w20_battery_set_key_sleep_mode(false);
    strncpy(s_ip5356_key_line, "idle", sizeof(s_ip5356_key_line) - 1);
    W20_BAT_LOGLN(F("[KEY] high -> clear key_sleep"));
#if USE_IP5356_CHARGER
    s_ip5356_periodic_reprobe_ms = now;
    ui_ip5356_runtime_reprobe_from_key("key_rise");
#endif
#endif
}

int ui_other_get_LTR(int *ch0, int *ch1, int *ps)
{
    // if(ch0 != NULL) *ch0 = lv_rand(0, LCD_VER_SIZE);
    // if(ch1 != NULL) *ch1 = lv_rand(0, LCD_VER_SIZE);
    // if(ps  != NULL) *ps  = lv_rand(0, LCD_VER_SIZE);

    if((ch0 != NULL) && (ch1 != NULL) && (ps != NULL))
    {
        *ch0 = LTR_553ALS_get_channel(0);
        *ch1 = LTR_553ALS_get_channel(1);
        *ps  = LTR_553ALS_get_ps();
    }
    else
    {
        Serial.printf("[%d] %s : Argument cannot be empty", __LINE__, __FILE__);
    }
    return 1;
}

int ui_other_get_gyro(float *gyro_x, float *gyro_y, float *gyro_z)
{
    // if(gyro_x != NULL) *gyro_x = lv_rand(0, LCD_VER_SIZE);
    // if(gyro_y != NULL) *gyro_y = lv_rand(0, LCD_VER_SIZE);
    // if(gyro_z != NULL) *gyro_z = lv_rand(0, LCD_VER_SIZE);

    if((gyro_x != NULL) && (gyro_x != NULL) && (gyro_x != NULL))
    {
        BHI260AP_get_val(2, gyro_x, gyro_y, gyro_z);
    }
    else
    {
        Serial.printf("[%d] %s : Argument cannot be empty", __LINE__, __FILE__);
    }
    return 1;
}

//************************************[ screen 8 ]****************************************** A7682E
#if USE_A7682E
#include <TinyGsmClient.h>
extern TinyGsm modem;
extern TaskHandle_t a7682_handle;
#endif

bool ui_a7682_at_cb(const char *at_cmd)
{
#if USE_A7682E
    printf("[A7682E] at cmd: %s\n", at_cmd);

    modem.sendAT("+CTTSPARAM=1,3,0,1,1");

    delay(100);

    modem.sendAT("+CTTS=2,\"1234567890\"");

    return false;
#else
    (void)at_cmd;
    return false;
#endif
}

void ui_a7682_call(const char *number)
{
#if USE_A7682E
    char buf[32];
    lv_snprintf(buf, 32, "D%s;", number);
    printf("[A7682E] at cmd: %s\n", buf);

    modem.sendAT(buf);
    delay(100);
#else
    (void)number;
#endif
}

void ui_a7682_hang_up(void)
{
#if USE_A7682E
    modem.sendAT("+CHUP");
    delay(100);
#endif
}

void ui_a7682_loop_resume(void)
{
#if USE_A7682E
    /* setup() 里若未调用 A7682E_init()，a7682_handle 为 NULL，禁止 vTaskResume(NULL) */
    if (a7682_handle != NULL) {
        vTaskResume(a7682_handle);
    }
#endif
}

void ui_a7682_loop_suspend(void)
{
#if USE_A7682E
    if (a7682_handle != NULL) {
        vTaskSuspend(a7682_handle);
    }
#endif
}

//************************************[ screen 9 ]****************************************** Input

void ui_shutdown_on(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_power_shutdown_execute();
#else
    power_shutdown_execute();
#endif
}

//************************************[ screen 10 ]****************************************** PCM5102
#if USE_PCM5102A
#include "Audio.h"
extern Audio audio;
#endif

bool ui_pcm5102_cb(const char *at_cmd)
{
#if USE_PCM5102A
    audio.connecttoFS(SPIFFS, "/iphone_call.mp3");
    return true;
#else
    (void)at_cmd;
    return false;
#endif
}

void ui_pcm5102_stop(void)
{
#if USE_PCM5102A
    audio.stopSong();
#endif
}

// optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
// void audio_id3data(const char *info){  //id3 metadata
//     Serial.print("id3data     ");Serial.println(info);
// }
// void audio_eof_mp3(const char *info){  //end of file
//     Serial.print("eof_mp3     ");Serial.println(info);
// }
// void audio_showstation(const char *info){
//     Serial.print("station     ");Serial.println(info);
// }
// void audio_showstreamtitle(const char *info){
//     Serial.print("streamtitle ");Serial.println(info);
// }
// void audio_bitrate(const char *info){
//     Serial.print("bitrate     ");Serial.println(info);
// }
// void audio_commercial(const char *info){  //duration in sec
//     Serial.print("commercial  ");Serial.println(info);
// }
// void audio_icyurl(const char *info){  //homepage
//     Serial.print("icyurl      ");Serial.println(info);
// }
// void audio_lasthost(const char *info){  //stream URL played
//     Serial.print("lasthost    ");Serial.println(info);
// }
