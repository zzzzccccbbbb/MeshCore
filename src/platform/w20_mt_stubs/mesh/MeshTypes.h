#pragma once

#include "mesh/generated/meshtastic/portnums.pb.h"
#include <stdint.h>
#include <string.h>

#ifndef NODENUM_BROADCAST
#define NODENUM_BROADCAST 0xffffffffu
#endif

enum RxSource {
    RX_SRC_LOCAL = 0,
    RX_SRC_RADIO = 1,
    RX_SRC_USER = 2,
};

struct meshtastic_Data {
    meshtastic_PortNum portnum;
    struct {
        uint8_t bytes[237];
        uint8_t size;
    } payload;
};

struct meshtastic_MeshPacket {
    uint32_t to;
    uint32_t from;
    uint8_t channel;
    bool want_ack;
    meshtastic_Data decoded;
};
