#include "M19AdcButtons.h"

#ifndef KEYS_ADC_LADDER_PROFILE
  #define KEYS_ADC_LADDER_PROFILE 2
#endif

#if KEYS_ADC_LADDER_PROFILE == 1
  #define MV_DOWN_MAX    380
  #define MV_RIGHT_MIN   500
  #define MV_RIGHT_MAX   720
  #define MV_UP_MIN      850
  #define MV_UP_MAX      1000
  #define MV_LEFT_MIN    1200
  #define MV_LEFT_MAX    1450
  #define MV_CENTER_MIN  1550
  #define MV_CENTER_MAX  1800
#else
  /* Profile 2 — factory default for M19 board2 */
  #define MV_DOWN_MAX    380
  #define MV_RIGHT_MIN   400
  #define MV_RIGHT_MAX   660
  #define MV_UP_MIN      680
  #define MV_UP_MAX      990
  #define MV_LEFT_MIN    1020
  #define MV_LEFT_MAX    1320
  #define MV_CENTER_MIN  1360
  #define MV_CENTER_MAX  1650
#endif
#define MV_IDLE_MIN      3000

#define KEYS_ADC_AVG_SAMPLES  4
#define KEYS_ADC_DEBOUNCE_MS  25
#define MULTI_CLICK_WINDOW_MS 280

namespace M19Keys {

static int s_pin = 13;
static bool s_inited = false;
static Id s_candidate = NONE;
static Id s_stable = NONE;
static unsigned long s_candidate_since = 0;

static Id classifyMv(int mv) {
  if (mv < 0) return NONE;
  if (mv <= MV_DOWN_MAX) return DOWN;
  if (mv >= MV_RIGHT_MIN && mv <= MV_RIGHT_MAX) return RIGHT;
  if (mv >= MV_UP_MIN && mv <= MV_UP_MAX) return UP;
  if (mv >= MV_LEFT_MIN && mv <= MV_LEFT_MAX) return LEFT;
  if (mv >= MV_CENTER_MIN && mv <= MV_CENTER_MAX) return CENTER;
  if (mv >= MV_IDLE_MIN) return NONE;
  return NONE;  /* unknown gap → ignore */
}

static int readMv() {
  uint32_t sum = 0;
  for (int i = 0; i < KEYS_ADC_AVG_SAMPLES; i++) {
    sum += (uint32_t)analogRead(s_pin);
  }
  uint32_t raw = sum / KEYS_ADC_AVG_SAMPLES;
  /* 12-bit, 11dB ≈ 0..3.3V */
  return (int)((raw * 3300UL) / 4095UL);
}

void begin(int pin) {
#ifdef PIN_KEY_ADC
  if (pin < 0) pin = PIN_KEY_ADC;
#endif
  s_pin = pin;
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(s_pin, INPUT);
  s_inited = true;
  s_candidate = s_stable = NONE;
  s_candidate_since = 0;
}

void poll() {
  if (!s_inited) return;

  int mv = readMv();
  Id now = classifyMv(mv);
  unsigned long t = millis();

  if (now != s_candidate) {
    s_candidate = now;
    s_candidate_since = t;
    return;
  }
  if ((t - s_candidate_since) < KEYS_ADC_DEBOUNCE_MS) {
    return;
  }
  s_stable = s_candidate;
}

bool pressed(Id id) {
  return s_stable == id;
}

}  // namespace M19Keys

AdcMappedButton::AdcMappedButton(M19Keys::Id id, int long_press_millis, bool multiclick)
  : _id(id),
    _long_millis(long_press_millis),
    _multiclick(multiclick),
    prev(0),
    cancel(0),
    down_at(0),
    _click_count(0),
    _last_click_time(0),
    _multi_click_window(multiclick ? MULTI_CLICK_WINDOW_MS : 0),
    _pending_click(false) {}

void AdcMappedButton::begin() {
  M19Keys::begin();
}

bool AdcMappedButton::isPressed() const {
  return M19Keys::pressed(_id);
}

void AdcMappedButton::cancelClick() {
  cancel = 1;
  down_at = 0;
  _click_count = 0;
  _last_click_time = 0;
  _pending_click = false;
}

int AdcMappedButton::check(bool repeat_click) {
  M19Keys::poll();

  int event = BUTTON_EVENT_NONE;
  int btn = M19Keys::pressed(_id) ? 1 : 0;

  if (btn != prev) {
    if (btn) {
      down_at = millis();
    } else {
      if (_long_millis > 0) {
        if (down_at > 0 && (millis() - down_at) < (unsigned long)_long_millis) {
          _click_count++;
          _last_click_time = millis();
          _pending_click = true;
        }
      } else {
        _click_count++;
        _last_click_time = millis();
        _pending_click = true;
      }
      down_at = 0;
    }
    prev = btn;
  }

  if (!btn && cancel) {
    cancel = 0;
  }

  if (_long_millis > 0 && down_at > 0 && (millis() - down_at) >= (unsigned long)_long_millis) {
    if (_pending_click) {
      cancelClick();
    } else {
      event = BUTTON_EVENT_LONG_PRESS;
      down_at = 0;
      _click_count = 0;
      _last_click_time = 0;
      _pending_click = false;
    }
  }

  if (down_at > 0 && repeat_click) {
    if ((millis() - down_at) >= 700) {
      event = BUTTON_EVENT_CLICK;
    }
  }

  if (_pending_click && (millis() - _last_click_time) >= (unsigned long)_multi_click_window) {
    if (down_at > 0) {
      return event;
    }
    if (_multiclick) {
      switch (_click_count) {
        case 1: event = BUTTON_EVENT_CLICK; break;
        case 2: event = BUTTON_EVENT_DOUBLE_CLICK; break;
        default: event = BUTTON_EVENT_TRIPLE_CLICK; break;
      }
    } else {
      event = BUTTON_EVENT_CLICK;
    }
    if (cancel) {
      event = BUTTON_EVENT_NONE;
    }
    _click_count = 0;
    _last_click_time = 0;
    _pending_click = false;
  }

  return event;
}
