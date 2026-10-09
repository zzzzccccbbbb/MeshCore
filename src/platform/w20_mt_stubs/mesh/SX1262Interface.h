#pragma once

#include "mesh/RadioLibInterface.h"
#include <stdint.h>

class SX1262Interface : public RadioLibInterface
{
  public:
    SX1262Interface(LockingArduinoHal *, int8_t, int8_t, int8_t, int8_t) {}
    bool init() override { return false; }
};
