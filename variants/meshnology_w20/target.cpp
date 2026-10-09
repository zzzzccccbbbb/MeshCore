#include <Arduino.h>
#include "target.h"

MeshnologyW20Board board;

W20SemtechRadio radio_driver(board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

#if ENV_INCLUDE_GPS
  #include <helpers/sensors/MicroNMEALocationProvider.h>
  MicroNMEALocationProvider nmea = MicroNMEALocationProvider(Serial1, &rtc_clock);
  EnvironmentSensorManager sensors = EnvironmentSensorManager(nmea);
#else
  EnvironmentSensorManager sensors;
#endif

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);

  // EN already raised in setup(); reinforce settle before Semtech HAL
  pinMode(P_LORA_EN, OUTPUT);
  digitalWrite(P_LORA_EN, HIGH);
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(P_LORA_BUSY, INPUT_PULLUP);
  delay(100);

  const bool ok = radio_driver.std_init();
  if (!ok) {
    Serial.println("W20: Semtech LR2021 init failed; Mesh RF unavailable.");
    Serial.println("W20: continuing boot so Deck menu still works.");
  }
  return ok;
}

mesh::LocalIdentity radio_new_identity() {
  class EspRng : public mesh::RNG {
  public:
    void random(uint8_t *dest, size_t sz) override {
      for (size_t i = 0; i < sz; i++)
        dest[i] = (uint8_t)(esp_random() & 0xFF);
    }
  } rng;
  return mesh::LocalIdentity(static_cast<mesh::RNG *>(&rng));
}
