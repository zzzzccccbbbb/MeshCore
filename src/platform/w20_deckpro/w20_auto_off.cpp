#include "w20_auto_off.h"

#if defined(W20_MESHNOLOGY)

#include "ui_deckpro.h"
#include "ui_deckpro_port.h"
#include "w20_dbg_log.h"
#if defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#endif

#include <Arduino.h>
#include <Preferences.h>

extern "C" void ui_deckpro_request_auto_off_shutdown(void);

static Preferences s_prefs;
static bool s_prefs_ok = false;
static bool s_enabled = true;
static uint32_t s_last_activity_ms = 0;
static bool s_sleep_armed = false;
static bool s_shutdown_armed = false;

void w20_auto_off_init(void)
{
    s_prefs_ok = s_prefs.begin("w20aoff", false);
    if (s_prefs_ok && s_prefs.isKey("en")) {
        s_enabled = s_prefs.getBool("en", true);
    } else {
        s_enabled = true;
        if (s_prefs_ok)
            s_prefs.putBool("en", true);
    }
    s_last_activity_ms = millis();
    s_sleep_armed = false;
    s_shutdown_armed = false;
    W20_UI_LOG("auto_off init en=%d sleep=%dmin off=%dmin", s_enabled ? 1 : 0,
               (int)W20_AUTO_SLEEP_MINUTES, (int)W20_AUTO_OFF_MINUTES);
}

bool w20_auto_off_get_enabled(void)
{
    return s_enabled;
}

void w20_auto_off_set_enabled(bool enabled)
{
    if (enabled != s_enabled) {
        s_enabled = enabled;
        if (s_prefs_ok)
            s_prefs.putBool("en", s_enabled);
        W20_UI_LOG("auto_off set en=%d", s_enabled ? 1 : 0);
    }
    s_last_activity_ms = millis();
    s_sleep_armed = false;
    s_shutdown_armed = false;
}

void w20_auto_off_kick(void)
{
    s_last_activity_ms = millis();
    s_sleep_armed = false;
    s_shutdown_armed = false;
}

void w20_auto_off_poll(void)
{
    if (!s_enabled || s_shutdown_armed)
        return;

#if defined(USE_W20_DECKPRO_SHELL)
    const bool sleeping = w20_deck_sleep_active();
#else
    const bool sleeping = false;
#endif

    /* 醒着时充电则暂停计时；睡眠中不碰 I2C（Wire 可能已 end） */
    if (!sleeping && (ui_battery_charging() || ui_battery_vbus_in())) {
        s_last_activity_ms = millis();
        return;
    }

    const uint32_t now = millis();
    const uint32_t idle_ms = (uint32_t)(now - s_last_activity_ms);
    const uint32_t sleep_need_ms = (uint32_t)W20_AUTO_SLEEP_MINUTES * 60000u;
    const uint32_t off_need_ms = (uint32_t)W20_AUTO_OFF_MINUTES * 60000u;

    if (idle_ms >= off_need_ms) {
        s_shutdown_armed = true;
        s_sleep_armed = true;
        W20_UI_LOG("auto_off timeout %d min -> SCREEN9", (int)W20_AUTO_OFF_MINUTES);
        ui_deckpro_request_auto_off_shutdown();
        return;
    }

    if (idle_ms >= sleep_need_ms && !s_sleep_armed && !sleeping) {
        s_sleep_armed = true;
        W20_UI_LOG("auto_off idle %d min -> deck sleep", (int)W20_AUTO_SLEEP_MINUTES);
#if defined(USE_W20_DECKPRO_SHELL)
        ui_deckpro_enter_board_sleep();
#endif
    }
}

#else

void w20_auto_off_init(void) {}
bool w20_auto_off_get_enabled(void) { return false; }
void w20_auto_off_set_enabled(bool) {}
void w20_auto_off_kick(void) {}
void w20_auto_off_poll(void) {}

#endif
