/**
 * Stubs for Deck Pro peripherals not wired in Meshtastic merge build.
 * LoRa/GPS: peri_lora.cpp + peri_gps.cpp + w20_*_arbiter (not stubbed).
 */
#include "peripheral.h"

bool BHI260AP_init(void)
{
    return false;
}
void BHI260AP_get_val(int, float *, float *, float *) {}

bool LTR553_init(void)
{
    return false;
}
uint16_t LTR_553ALS_get_channel(int)
{
    return 0;
}
uint16_t LTR_553ALS_get_ps(void)
{
    return 0;
}
