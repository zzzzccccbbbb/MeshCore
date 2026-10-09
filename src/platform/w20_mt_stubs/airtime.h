#pragma once

class AirTime
{
  public:
    bool isTxAllowedAirUtil() { return true; }
    bool isTxAllowedChannelUtil(bool = false) { return true; }
};

extern AirTime *airTime;
