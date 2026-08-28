#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "w10_pins.h"
#include <helpers/ESP32Board.h>
#include <helpers/Mcp23017Helper.h>

class MeshnologyW10Board : public ESP32Board {
  SemaphoreHandle_t _spiMutex = nullptr;
  bool _pmuReady = false;

  void initPmu();
  void initGpsPins();

public:
  SPIClass sharedSpi{HSPI};

  MeshnologyW10Board() {}

  void begin();
  void lockSharedSpi();
  void unlockSharedSpi();

  uint32_t getIRQGpio() override {
    return 0;  // DIO1 is on MCP23017, not a direct GPIO
  }

  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1);
  void powerOff() override;
  uint16_t getBattMilliVolts() override;

  const char *getManufacturerName() const override {
    return "Meshnology W10";
  }
};
