#pragma once

#include <stdint.h>

typedef enum _meshtastic_PortNum {
    meshtastic_PortNum_UNKNOWN_APP = 0,
    meshtastic_PortNum_TEXT_MESSAGE_APP = 1,
    meshtastic_PortNum_POSITION_APP = 3,
} meshtastic_PortNum;
