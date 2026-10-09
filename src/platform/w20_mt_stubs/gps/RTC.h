#pragma once

#include <stdint.h>

#ifndef SEC_PER_DAY
#define SEC_PER_DAY 86400L
#endif
#ifndef SEC_PER_HOUR
#define SEC_PER_HOUR 3600L
#endif
#ifndef SEC_PER_MIN
#define SEC_PER_MIN 60L
#endif

enum class RTCQuality {
    RTCQualityNone = 0,
    RTCQualityDevice = 1,
    RTCQualityNTP = 2,
    RTCQualityGPS = 3,
};

inline uint32_t getTime() { return 0; }

inline uint32_t getValidTime(RTCQuality, bool = false) { return 0; }

inline void perhapsSetRTC(RTCQuality, struct tm *) {}
