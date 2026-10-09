#pragma once

#include <Arduino.h>
#include <MeshnologyW20Board.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include <helpers/w20/W20SemtechRadio.h>
#ifdef DISPLAY_CLASS
  #include <platform/w20_deckpro_shell/W20ShellEpdDisplay.h>
  #include <helpers/ui/MomentaryButton.h>
#endif

extern MeshnologyW20Board board;
extern W20SemtechRadio radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
