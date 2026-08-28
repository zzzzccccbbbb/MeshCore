#pragma once

#include <RadioLib.h>
#include "Mcp23017Helper.h"

typedef void (*SpiLockFn)();

class Mcp23017Hal : public ArduinoHal {
  SpiLockFn _lockFn = nullptr;
  SpiLockFn _unlockFn = nullptr;

public:
  explicit Mcp23017Hal(SPIClass &spi, SpiLockFn lockFn = nullptr, SpiLockFn unlockFn = nullptr,
    SPISettings settings = RADIOLIB_DEFAULT_SPI_SETTINGS);

  void pinMode(uint32_t pin, uint32_t mode) override;
  void digitalWrite(uint32_t pin, uint32_t value) override;
  uint32_t digitalRead(uint32_t pin) override;
  void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;
  void spiBeginTransaction() override;
  void spiEndTransaction() override;
};
