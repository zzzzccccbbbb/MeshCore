#pragma once

#include "RadioInterface.h"
#include "mesh/MeshTypes.h"

class Router
{
  public:
    meshtastic_MeshPacket *allocForSending() { return nullptr; }
    void addInterface(RadioInterface *) {}
};

extern Router *router;
