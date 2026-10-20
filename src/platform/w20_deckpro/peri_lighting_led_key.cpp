/**
 * PIN_KEY_LED (GPIO48)：
 * 短按 — 关灯↔常亮；闪烁模式下短按 — 关灯。
 * 长按（按住满 800 ms）— 非闪烁时立即进入连续闪烁（180 ms 亮/灭）；已在闪烁时长按忽略。
 * GPIO CHANGE 中断记边沿/按下时刻；业务逻辑在 peri_lighting_led_key_poll()。
 */

#include "peri_lighting_led_key.h"
#include "utilities.h"
#include "ui_deckpro_port.h"
#include "w20_dbg_log.h"
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro_shell/w20_sos_beacon.h"
#endif

#include <Arduino.h>

#ifndef LED_KEY_SHORT_MAX_MS
#define LED_KEY_SHORT_MAX_MS 450u
#endif
#ifndef LED_KEY_LONG_MIN_MS
#define LED_KEY_LONG_MIN_MS 800u
#endif
#ifndef LED_KEY_DEBOUNCE_MS
#define LED_KEY_DEBOUNCE_MS 25u
#endif
#ifndef LIGHTING_BLINK_ON_MS
#define LIGHTING_BLINK_ON_MS 180u
#endif
#ifndef LIGHTING_BLINK_OFF_MS
#define LIGHTING_BLINK_OFF_MS 180u
#endif

typedef enum {
    LED_KEY_MODE_OFF = 0,
    LED_KEY_MODE_STEADY,
    LED_KEY_MODE_BLINK,
    LED_KEY_MODE_NOTIFY,
} led_key_mode_t;

static int prev_led_raw;
static bool tracking_press;
static bool press_long_fired;
static uint32_t press_start_ms;

static led_key_mode_t lamp_mode;

static bool blink_phase_on;
static uint32_t blink_next_ms;
static uint8_t notify_phases_left;

static bool s_led_isr_attached;
static portMUX_TYPE s_led_key_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile bool s_pressed;
static volatile uint32_t s_press_start_ms_isr;
static volatile uint32_t s_last_isr_ms;
static volatile bool s_pending_down;
static volatile bool s_pending_up;

static bool led_key_hw_pressed(void)
{
    return digitalRead(PIN_KEY_LED) == LOW;
}

static void IRAM_ATTR led_key_isr(void)
{
    const uint32_t now = millis();
    if (s_last_isr_ms != 0 && (now - s_last_isr_ms) < LED_KEY_DEBOUNCE_MS) {
        return;
    }
    s_last_isr_ms = now;

    const bool pressed = (digitalRead(PIN_KEY_LED) == LOW);
    portENTER_CRITICAL_ISR(&s_led_key_mux);
    if (pressed && !s_pressed) {
        s_pressed = true;
        s_press_start_ms_isr = now;
        s_pending_down = true;
    } else if (!pressed && s_pressed) {
        s_pressed = false;
        s_pending_up = true;
    }
    portEXIT_CRITICAL_ISR(&s_led_key_mux);
}

static void led_key_attach_isr(void)
{
    if (s_led_isr_attached || PIN_KEY_LED < 0) {
        return;
    }
    pinMode(PIN_KEY_LED, INPUT_PULLUP);
    s_pressed = led_key_hw_pressed();
    s_last_isr_ms = 0;
    portENTER_CRITICAL(&s_led_key_mux);
    s_pending_down = false;
    s_pending_up = false;
    portEXIT_CRITICAL(&s_led_key_mux);
    attachInterrupt(digitalPinToInterrupt(PIN_KEY_LED), led_key_isr, CHANGE);
    s_led_isr_attached = true;
}

static void led_key_consume_edges(bool *down, bool *up)
{
    *down = false;
    *up = false;
    portENTER_CRITICAL(&s_led_key_mux);
    if (s_pending_down) {
        s_pending_down = false;
        *down = true;
    }
    if (s_pending_up) {
        s_pending_up = false;
        *up = true;
    }
    portEXIT_CRITICAL(&s_led_key_mux);
}

static void led_key_clear_pending_edges(void)
{
    portENTER_CRITICAL(&s_led_key_mux);
    s_pending_down = false;
    s_pending_up = false;
    portEXIT_CRITICAL(&s_led_key_mux);
}

static void blink_enter(void)
{
    lamp_mode = LED_KEY_MODE_BLINK;
    blink_phase_on = true;
    ui_lighting_lamp_hw_only(true);
    blink_next_ms = millis() + LIGHTING_BLINK_ON_MS;
    ui_setting_set_led_display_state(true);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_sos_beacon_start();
#endif
}

static void blink_tick(void)
{
    if (lamp_mode != LED_KEY_MODE_BLINK) {
        return;
    }
    uint32_t now = millis();
    if (now < blink_next_ms) {
        return;
    }
    blink_phase_on = !blink_phase_on;
    ui_lighting_lamp_hw_only(blink_phase_on);
    blink_next_ms = now + (blink_phase_on ? LIGHTING_BLINK_ON_MS : LIGHTING_BLINK_OFF_MS);
}

static void notify_tick(void)
{
    if (lamp_mode != LED_KEY_MODE_NOTIFY) {
        return;
    }
    uint32_t now = millis();
    if (now < blink_next_ms) {
        return;
    }
    if (notify_phases_left <= 1) {
        lamp_mode = LED_KEY_MODE_OFF;
        ui_lighting_lamp_hw_only(false);
        notify_phases_left = 0;
        return;
    }
    notify_phases_left--;
    blink_phase_on = !blink_phase_on;
    ui_lighting_lamp_hw_only(blink_phase_on);
    blink_next_ms = now + (blink_phase_on ? LIGHTING_BLINK_ON_MS : LIGHTING_BLINK_OFF_MS);
}

void peri_lighting_lamp_flash_twice(void)
{
    if (PIN_LIGHTING_LAMP < 0) {
        return;
    }
    /* 不在此处停 SOS：自己回环也会走 newMsg→flash，误停；停发只走 w20_sos_on_* */
    if (lamp_mode == LED_KEY_MODE_BLINK) {
        return; /* SOS 闪烁中：留给 on_channel_text / 按键停 */
    }
    lamp_mode = LED_KEY_MODE_NOTIFY;
    notify_phases_left = 4;
    blink_phase_on = true;
    ui_lighting_lamp_hw_only(true);
    blink_next_ms = millis() + LIGHTING_BLINK_ON_MS;
}

void peri_lighting_led_key_sync_from_ui(bool lighting_on)
{
    if (!lighting_on) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        w20_sos_beacon_stop();
#endif
        lamp_mode = LED_KEY_MODE_OFF;
    } else {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        w20_sos_beacon_stop();
#endif
        lamp_mode = LED_KEY_MODE_STEADY;
    }
}

void peri_lighting_led_key_init(void)
{
    if (PIN_KEY_LED < 0 || PIN_LIGHTING_LAMP < 0) {
        return;
    }
    led_key_attach_isr();
    prev_led_raw = s_pressed ? 1 : 0;
    tracking_press = prev_led_raw != 0;
    press_long_fired = false;
    if (tracking_press) {
        press_start_ms = s_press_start_ms_isr;
    }
    lamp_mode = LED_KEY_MODE_OFF;
}

void peri_lighting_led_key_poll(void)
{
    if (PIN_KEY_LED < 0 || PIN_LIGHTING_LAMP < 0) {
        return;
    }
    if (!s_led_isr_attached) {
        led_key_attach_isr();
    }

    blink_tick();
    notify_tick();

    const int raw = s_pressed ? 1 : 0;

    if (lamp_mode == LED_KEY_MODE_NOTIFY) {
        led_key_clear_pending_edges();
        prev_led_raw = raw;
        return;
    }

    bool edge_down = false;
    bool edge_up = false;
    led_key_consume_edges(&edge_down, &edge_up);

    if (edge_down) {
        press_start_ms = s_press_start_ms_isr;
        tracking_press = true;
        press_long_fired = false;
    }

    if (raw && tracking_press && !press_long_fired && lamp_mode != LED_KEY_MODE_BLINK) {
        if (millis() - press_start_ms >= LED_KEY_LONG_MIN_MS) {
            blink_enter();
            press_long_fired = true;
            W20_LED_KEY_LOGLN("[LED_KEY] long: blink ON");
        }
    }

    if (edge_up && tracking_press) {
        tracking_press = false;
        if (!press_long_fired) {
            uint32_t dt = millis() - press_start_ms;
            if (dt < LED_KEY_SHORT_MAX_MS) {
                if (lamp_mode == LED_KEY_MODE_STEADY || lamp_mode == LED_KEY_MODE_BLINK) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
                    if (lamp_mode == LED_KEY_MODE_BLINK) {
                        w20_sos_beacon_stop();
                    }
#endif
                    ui_setting_set_led_status(false);
                    Serial.println(lamp_mode == LED_KEY_MODE_BLINK ? "[LED_KEY] short: blink OFF"
                                                                   : "[LED_KEY] short: steady OFF");
                } else {
                    ui_setting_set_led_status(true);
                    W20_LED_KEY_LOGLN("[LED_KEY] short: steady ON");
                }
            }
        }
        press_long_fired = false;
    }

    prev_led_raw = raw;

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_sos_beacon_poll();
#endif
}
