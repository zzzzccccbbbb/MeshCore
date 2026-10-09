#pragma once

#include <stdint.h>

class Channels
{
  public:
    uint8_t getPrimaryIndex() const { return 0; }
    bool isDefaultChannel(uint8_t) const { return true; }
};

extern Channels channels;
