#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>

class MeshnologyW13Board : public NRF52BoardDCDC {
protected:
  uint8_t btn_prev_state;

public:
  MeshnologyW13Board() : NRF52Board("Meshnology W13 OTA") {}
  void begin();

#if defined(P_LORA_TX_LED)
  void onBeforeTransmit() override {
    digitalWrite(P_LORA_TX_LED, HIGH);
  }
  void onAfterTransmit() override {
    digitalWrite(P_LORA_TX_LED, LOW);
  }
#endif

  uint16_t getBattMilliVolts() override {
    analogReadResolution(12);
    analogReference(AR_INTERNAL);
    pinMode(VBAT_ENABLE, OUTPUT);
    digitalWrite(VBAT_ENABLE, HIGH);
    delay(10);
    int adcvalue = analogRead(PIN_VBAT_READ);
    digitalWrite(VBAT_ENABLE, LOW);
    return (adcvalue * ADC_MULTIPLIER * AREF_VOLTAGE) / 4.096;
  }

  const char* getManufacturerName() const override {
    return "Meshnology W13";
  }

  void powerOff() override {
    sd_power_system_off();
  }
};
