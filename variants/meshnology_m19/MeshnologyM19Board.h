#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/rtc_io.h>
#include "m19_pins.h"
#include <helpers/ESP32Board.h>
#include <helpers/Mcp23017Helper.h>

#ifndef ADC_MULTIPLIER
  /* Divider 390k/100k = 4.9; factory one-point gain ≈1.078 → ~5.28 */
  #define ADC_MULTIPLIER 5.28f
#endif

class MeshnologyM19Board : public ESP32Board {
  SemaphoreHandle_t _spiMutex = nullptr;
  float adc_mult = ADC_MULTIPLIER;
  uint32_t _batt_filt_mv = 0;
  bool _batt_filt_ok = false;

  void mcpWrite(uint32_t mcpPin, uint8_t level);
  void mcpPinMode(uint32_t mcpPin, uint8_t mode);
  void powerRailsOn();
  void initGpsPins();

public:
  SPIClass sharedSpi{HSPI};

  MeshnologyM19Board() {}

  void begin();
  void lockSharedSpi();
  void unlockSharedSpi();

  void lcdResetPulse();
  void setGpsPower(bool on);

  void onBeforeTransmit(void) override;
  void onAfterTransmit(void) override;

  uint32_t getIRQGpio() override { return P_LORA_DIO_1; }

  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1);
  void powerOff() override;
  uint16_t getBattMilliVolts() override;
  bool isExternalPowered() override;

  bool setAdcMultiplier(float multiplier) override {
    adc_mult = (multiplier == 0.0f) ? ADC_MULTIPLIER : multiplier;
    return true;
  }
  float getAdcMultiplier() const override { return adc_mult; }

  const char *getManufacturerName() const override {
    return "Meshnology M19";
  }
};
