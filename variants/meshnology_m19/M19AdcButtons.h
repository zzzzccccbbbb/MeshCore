#pragma once

#include <Arduino.h>
#include <helpers/ui/MomentaryButton.h>

/**
 * M19 five-way M1-5246G on GPIO13 resistor ladder.
 * AdcMappedButton mirrors MomentaryButton API for UI_HAS_JOYSTICK.
 */
namespace M19Keys {
  enum Id : uint8_t {
    NONE = 0,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    CENTER,
  };

  void begin(int pin = 13);
  void poll();
  bool pressed(Id id);
}

class AdcMappedButton {
  M19Keys::Id _id;
  int _long_millis;
  bool _multiclick;
  int8_t prev;
  int8_t cancel;
  unsigned long down_at;
  uint8_t _click_count;
  unsigned long _last_click_time;
  int _multi_click_window;
  bool _pending_click;

public:
  AdcMappedButton(M19Keys::Id id, int long_press_millis = 1000, bool multiclick = false);

  void begin();
  int check(bool repeat_click = false);
  void cancelClick();
  bool isPressed() const;
};
