#pragma once

class NimbleBluetooth
{
  public:
    bool isActive() const { return false; }
    void setup() {}
    void deinit() {}
};

extern NimbleBluetooth *nimbleBluetooth;
