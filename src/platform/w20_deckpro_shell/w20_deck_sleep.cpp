/**
 * W20 Deck 统一软睡眠：关人机外设，保留 MeshCore Semtech LoRa RX；
 * GPS 交回 mesh arbiter；BLE 在暗屏时深度省电（见 w20_deck_sleep_suppress_ble_in_dark）。
 */
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"
#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro_shell/w20_ui_debug.h"
#include "platform/w20_deckpro/peri_power.h"
#include "platform/w20_deckpro/peripheral.h"
#include "graphics/Screen.h"
#include "graphics/EInkDisplay2.h"
#include "peri_lighting_led_key.h"
#include "platform/w20_deckpro/w20_wifi_ota.h"
#include "ui_deckpro.h"
#include "ui_deckpro_port.h"
#include "utilities.h"
#include "PowerFSM.h"
#include "main.h"
#include "target_specific.h"
#include "configuration.h"

#include <Arduino.h>
#include <WiFi.h>

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
#include "nimble/NimbleBluetooth.h"
#endif

extern graphics::Screen *screen;

static bool s_deck_sleep = false;
static bool s_menu_sleep_dark = false;
static int s_boot_prev = 0;
static uint32_t s_suppress_boot_nav_until = 0;
static bool s_defer_screen_run = false;

bool w20_deck_sleep_defer_screen_run(void)
{
    return s_defer_screen_run;
}

void w20_deck_sleep_set_defer_screen_run(bool defer)
{
    s_defer_screen_run = defer;
}

/* 需覆盖 wake 内 1×E-Ink 全刷(~1.5s) + UserButton 去抖 */
#ifndef W20_DECK_SLEEP_BOOT_NAV_SUPPRESS_MS
#define W20_DECK_SLEEP_BOOT_NAV_SUPPRESS_MS 5000
#endif

void w20_deck_sleep_arm_boot_nav_suppress(void)
{
    s_suppress_boot_nav_until = millis() + W20_DECK_SLEEP_BOOT_NAV_SUPPRESS_MS;
}

static void deck_rgb_off(void)
{
    if (PIN_RGB_R >= 0) {
        pinMode(PIN_RGB_R, OUTPUT);
        digitalWrite(PIN_RGB_R, RGB_R_ACTIVE_LEVEL ? LOW : HIGH);
    }
    if (PIN_RGB_B >= 0) {
        pinMode(PIN_RGB_B, OUTPUT);
        digitalWrite(PIN_RGB_B, RGB_B_ACTIVE_LEVEL ? LOW : HIGH);
    }
}

void w20_deck_periph_sleep_off(void)
{
    factory_pause_flush_timer();

    deck_rgb_off();
    ui_lighting_lamp_hw_only(false);
    peri_lighting_led_key_sync_from_ui(false);

#if W20_DECK_SLEEP_WIFI_OFF
    if (!w20_wifi_ota_is_running() && !w20_wifi_ota_is_busy() && WiFi.getMode() != WIFI_OFF) {
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
    }
#endif

    power_deck_sleep_enter_periph();

    w20_mesh_gps_shutdown();

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (nimbleBluetooth) {
        nimbleBluetooth->deinit();
    }
#endif

}

void w20_deck_periph_sleep_on(void)
{
    power_deck_sleep_exit_periph();

    if (w20_shell_mesh_screen_allowed()) {
        w20_gps_arbiter_request_mesh();
        w20_mesh_gps_try_init();
    }

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (config.bluetooth.enabled) {
        setBluetoothEnable(true);
    }
#endif
}

bool w20_deck_sleep_suppress_ble_in_dark(void)
{
    return s_menu_sleep_dark;
}

bool w20_deck_skip_auto_dark_screen_off(void)
{
    /* 手动 Sleep（EVENT_W20_DECK_SLEEP）须进 DARK；仅九宫格 Deck 菜单禁止自动灭屏。 */
    if (s_menu_sleep_dark) {
        return false;
    }
    return !w20_shell_mesh_screen_allowed();
}

bool w20_deck_sleep_active(void)
{
    return s_deck_sleep;
}

void w20_deck_sleep_enter(void)
{
    if (s_deck_sleep) {
        return;
    }
    s_deck_sleep = true;
    s_menu_sleep_dark = false;
    s_boot_prev = ui_input_read_boot_key();
    ui_deckpro_pause_aux_timers();
    /* Mesh 超时 / 菜单 Sleep：统一刷开机 logo + 底部 Sleep，再 hold 画面 */
    w20_deckpro_sleep_prompt_flush();
    w20_deck_periph_sleep_off();
    W20_UI_LOG("deck_sleep enter (mesh_allowed=%d)", w20_shell_mesh_screen_allowed() ? 1 : 0);
}

bool w20_deck_sleep_should_suppress_boot_nav(input_broker_event ev)
{
    if (ev != INPUT_BROKER_USER_PRESS) {
        return false;
    }
    return millis() < s_suppress_boot_nav_until;
}

void w20_deck_sleep_wake(void)
{
    if (!s_deck_sleep) {
        return;
    }
    s_deck_sleep = false;
    s_boot_prev = ui_input_read_boot_key();
    w20_deck_periph_sleep_on();
    if (screen != nullptr) {
        const bool on_mesh = w20_shell_mesh_screen_allowed();
        if (on_mesh) {
            /* 只全刷一次：挡住随后 FIXED/顶栏电量触发的叠刷约 4s */
            w20_deck_sleep_set_defer_screen_run(true);
            w20_shell_set_skip_mesh_idle_force(true);
            screen->w20SyncSetOn(true);
            screen->forceDisplay(true);
            w20_deck_sleep_set_defer_screen_run(false);
            /* skip 延后清除在 shell_loop（见 w20_shell_clear_wake_epd_skip） */
            w20_shell_arm_wake_epd_skip_ms(4000);
        } else {
            screen->w20SyncSetOn(true);
            w20_deckpro_present_menu_on_epd();
        }
    }
    s_menu_sleep_dark = false;
    /* 离开 DARK，避免 screen_on_secs 定时 darkEnter 再次关屏；Boot 导航由 suppress 窗口挡住。 */
    powerFSM.trigger(EVENT_PRESS);
    runASAP = true;
    W20_UI_LOG("deck_sleep wake");
}

void w20_deck_sleep_poll_boot(void)
{
    if (!s_deck_sleep) {
        return;
    }
    const int boot = ui_input_read_boot_key();
    if (boot && !s_boot_prev) {
        W20_UI_LOG("deck_sleep boot press");
        w20_deck_sleep_arm_boot_nav_suppress();
        w20_deck_sleep_wake();
        /* 勿 EVENT_PRESS：会进 ON/POWER 且 UserButton 再发 USER_PRESS→showNextFrame 跳 messages */
    }
    s_boot_prev = boot;
}

void w20_deck_sleep_request_dark(void)
{
    s_menu_sleep_dark = true;
    powerFSM.trigger(EVENT_W20_DECK_SLEEP);
}

bool w20_deck_board_sleep_active(void)
{
    return w20_deck_sleep_active();
}

void w20_deck_board_sleep_arm(void)
{
    w20_deck_sleep_enter();
}

void w20_deck_board_sleep_wake(void)
{
    w20_deck_sleep_wake();
}

#endif
