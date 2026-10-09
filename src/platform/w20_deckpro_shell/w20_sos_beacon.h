#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Start periodic SOS broadcast (Primary channel) while LED blink mode is active. */
void w20_sos_beacon_start(void);
/** Stop SOS broadcast. */
void w20_sos_beacon_stop(void);
/** Call from peri_lighting_led_key_poll(); sends every ~2.5 s when active and mesh group ready. */
void w20_sos_beacon_poll(void);
bool w20_sos_beacon_active(void);

#ifdef __cplusplus
}
#endif
