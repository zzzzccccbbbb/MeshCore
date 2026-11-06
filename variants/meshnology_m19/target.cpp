#include <Arduino.h>
#include "target.h"
#include <helpers/Mcp23017Hal.h>

MeshnologyM19Board board;

static Mcp23017Hal radioHal(
  board.sharedSpi,
  []() { board.lockSharedSpi(); },
  []() { board.unlockSharedSpi(); }
);
static Module radioModule(&radioHal, P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
RADIO_CLASS radio(&radioModule);

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

#if ENV_INCLUDE_GPS
  #include <helpers/sensors/MicroNMEALocationProvider.h>
  /* GPS EN/RST are on MCP23017 — Board manages them; MicroNMEA gets -1/-1. */
  MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1, &rtc_clock, -1, -1);
  EnvironmentSensorManager sensors = EnvironmentSensorManager(nmea);
#else
  EnvironmentSensorManager sensors;
#endif

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display(&board);
  #if UI_HAS_JOYSTICK
    #ifndef PIN_BACK_BTN
      #define PIN_BACK_BTN 0
    #endif
    AdcMappedButton user_btn(M19Keys::CENTER, 1000, false);
    AdcMappedButton joystick_left(M19Keys::LEFT, 1000, false);
    AdcMappedButton joystick_right(M19Keys::RIGHT, 1000, false);
    MomentaryButton back_btn(PIN_BACK_BTN, 1000, true, false, true);
  #else
    MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
  #endif
#endif

/* Same FEM DIO map as factory LR2021 shield / W12. */
const uint32_t rfswitch_dios[] = {
  RADIOLIB_LR2021_DIO5,   // 2.4G TX_EN
  RADIOLIB_LR2021_DIO6,   // 2.4G RX_EN
  RADIOLIB_LR2021_DIO9,   // Sub-G CTX
  RADIOLIB_LR2021_DIO10,  // Sub-G CPS
  RADIOLIB_LR2021_DIO11,  // Sub-G CSD
};

static const Module::RfSwitchMode_t rfswitch_table[] = {
  //                    DIO5   DIO6  DIO9  DIO10  DIO11
  { LR2021::MODE_STBY,  { LOW,  LOW,  LOW,  LOW,  LOW  } },
  { LR2021::MODE_RX,    { LOW,  LOW,  LOW,  LOW,  HIGH } },
  { LR2021::MODE_TX,    { LOW,  LOW,  HIGH, HIGH, HIGH } },
  { LR2021::MODE_RX_HF, { LOW,  HIGH, LOW,  LOW,  LOW  } },
  { LR2021::MODE_TX_HF, { HIGH, LOW,  LOW,  LOW,  LOW  } },
  END_OF_MODE_TABLE,
};

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);

#if ENV_INCLUDE_GPS
  Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
#endif

  bool ok = radio.std_init(&board.sharedSpi);
  if (!ok) return false;

#ifdef RF_SWITCH_TABLE
  radio.setRfSwitchTable(rfswitch_dios, rfswitch_table);
#endif

  return true;
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
