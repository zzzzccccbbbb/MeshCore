/**
 * W20 统一最低功耗关机（Deck Shutdown 菜单）。
 */
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "platform/w20_deckpro_shell/w20_power_shutdown.h"
#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro_shell/w20_lora_arbiter.h"
#include "platform/w20_deckpro/peri_power.h"
#include "mesh/NodeDB.h"
#include "main.h"

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
#include "nimble/NimbleBluetooth.h"
#endif

#ifdef USE_EINK
#include "graphics/EInkDisplay2.h"
#endif

#include <Arduino.h>

void w20_power_shutdown_execute(void)
{
    Serial.println("W20 shutdown: begin");

    if (nodeDB) {
        nodeDB->saveToDisk();
        Serial.println("W20 shutdown: config saved");
    }

    w20_mesh_radio_shutdown();
    for (int i = 0; i < 20; i++) {
        w20_lora_arbiter_tick();
        delay(5);
    }

    w20_mesh_gps_shutdown();

#if !MESHTASTIC_EXCLUDE_BLUETOOTH
    if (nimbleBluetooth) {
        nimbleBluetooth->deinit();
        Serial.println("W20 shutdown: BLE deinit");
    }
#endif

#ifdef USE_EINK
    if (screen != nullptr) {
        OLEDDisplay *oled = screen->getDisplayDevice();
        if (oled != nullptr) {
            auto *eink = static_cast<EInkDisplay *>(oled);
            eink->hibernateHardware();
            Serial.println("W20 shutdown: Meshtastic EPD hibernate");
        }
    }
#endif

    power_shutdown_execute();
}

#endif
