#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <MeshnologyM19Board.h>
#include <helpers/radiolib/CustomLR2021Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#ifdef DISPLAY_CLASS
  #include <helpers/ui/ST7789LCDDisplay.h>
  #include <helpers/ui/MomentaryButton.h>
  #if UI_HAS_JOYSTICK
    #include "M19AdcButtons.h"
  #endif
#endif

extern MeshnologyM19Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  #if UI_HAS_JOYSTICK
    extern AdcMappedButton user_btn;
    extern AdcMappedButton joystick_left;
    extern AdcMappedButton joystick_right;
    extern MomentaryButton back_btn;
  #else
    extern MomentaryButton user_btn;
  #endif
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
