#pragma once

#include "GPS.h"

namespace meshtastic
{

class GPSStatus
{
  public:
    void observe(void *) {}
    template <typename T> void observe(Observable<T> *) {}
};

} // namespace meshtastic

extern meshtastic::GPSStatus *gpsStatus;
