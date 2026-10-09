#pragma once

#include "configuration.h"
#include <stdint.h>

#ifndef SEGMENT_CONFIG
#define SEGMENT_CONFIG 1
#endif

class NodeDB
{
  public:
    void saveToDisk(uint32_t segments = 0) { (void)segments; }
};

extern NodeDB *nodeDB;
extern meshtastic_Config config;
