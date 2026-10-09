#pragma once

#include <stdint.h>

class HasBatteryLevel
{
  public:
    virtual ~HasBatteryLevel() {}
    virtual int getBatteryPercent() { return -1; }
    virtual uint16_t getBattVoltage() { return 0; }
    virtual bool isBatteryConnect() { return false; }
    virtual bool isVbusIn() { return false; }
    virtual bool isCharging() { return false; }
};
