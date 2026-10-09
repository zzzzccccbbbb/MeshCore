#ifndef __TEST_FACTORY_H__
#define __TEST_FACTORY_H__

/*********************************************************************************
 * Deck Pro factory header — Meshtastic build uses minimal globals (no modem/audio/SPIFFS).
 *********************************************************************************/
#include "peripheral.h"
#include "SPI.h"

#define DISP_REFR_MODE_FULL 0
#define DISP_REFR_MODE_PART 1

#ifndef TINY_GSM_MODEM_SIM7672
#define TINY_GSM_MODEM_SIM7672
#endif

extern bool peri_init_st[E_PERI_NUM_MAX];

union flush_buf_pixel {
    struct _byte {
        uint8_t b1 : 1;
        uint8_t b2 : 1;
        uint8_t b3 : 1;
        uint8_t b4 : 1;
        uint8_t b5 : 1;
        uint8_t b6 : 1;
        uint8_t b7 : 1;
        uint8_t b8 : 1;
    } bit;
    uint8_t full;
};

void disp_full_refr(void);

#endif
