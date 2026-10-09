/**
 * SOS under MeshCore Deck shell: LED blink only (peri_lighting_led_key).
 * No Meshtastic channel TX; MeshCore packet SOS can be added later.
 */
#include "configuration.h"
#include "platform/w20_deckpro_shell/w20_sos_beacon.h"

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include <Arduino.h>

static bool s_active = false;

void w20_sos_beacon_start(void)
{
    s_active = true;
    Serial.println("[SOS] LED-only (no mesh TX on MeshCore build)");
}

void w20_sos_beacon_stop(void)
{
    s_active = false;
}

void w20_sos_beacon_poll(void)
{
    /* TX not wired; LED handled by peri_lighting_led_key. */
}

bool w20_sos_beacon_active(void)
{
    return s_active;
}

#else

void w20_sos_beacon_start(void) {}
void w20_sos_beacon_stop(void) {}
void w20_sos_beacon_poll(void) {}
bool w20_sos_beacon_active(void)
{
    return false;
}

#endif
