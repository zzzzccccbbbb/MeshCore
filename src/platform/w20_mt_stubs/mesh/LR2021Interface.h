#pragma once

#include "RadioInterface.h"

class LR2021Interface : public RadioInterface
{
  public:
    bool init() override { return false; }
    void disable() override {}
    static void detachDioCallback() {}
};
