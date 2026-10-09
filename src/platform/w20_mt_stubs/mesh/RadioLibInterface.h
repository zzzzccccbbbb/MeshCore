#pragma once

#include "RadioInterface.h"
#include <Arduino.h>
#include <SPI.h>

class LockingArduinoHal
{
  public:
    LockingArduinoHal(SPIClass &, const SPISettings &) {}
};

class RadioLibInterface : public RadioInterface
{
  public:
    bool init() override { return false; }
    void disable() override {}
};
