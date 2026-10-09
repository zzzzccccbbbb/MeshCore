#pragma once

#include <stdint.h>

/** Minimal OLEDDisplay stand-in (ThingPulse API surface used by Deck shell). */
class OLEDDisplay
{
  public:
    virtual ~OLEDDisplay() {}
    virtual void display() {}
    virtual void setBrightness(uint8_t) {}
};
