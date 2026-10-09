#pragma once

#include <stdint.h>

template <typename T> class Observable
{
  public:
    void observe(void *) {}
};

class GPS
{
  public:
    Observable<int> newStatus;

    static GPS *createGps() { return nullptr; }
    void handoffSuspendForDeck() {}
    void handoffResumeAfterDeck() {}
};

extern GPS *gps;
