#pragma once

#include "mesh/MeshTypes.h"

class MeshService
{
  public:
    void sendToMesh(meshtastic_MeshPacket *, RxSource = RX_SRC_LOCAL, bool = false) {}
};

extern MeshService *service;
