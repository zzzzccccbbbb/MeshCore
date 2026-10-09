#pragma once

#include "OLEDDisplay.h"
#include <stdint.h>

class EInkDisplay : public OLEDDisplay
{
  public:
    void forceDisplay(uint32_t = 0) {}
    void hibernateHardware() {}
    bool isEInk() const { return true; }
};

typedef EInkDisplay EInkDisplay2;
