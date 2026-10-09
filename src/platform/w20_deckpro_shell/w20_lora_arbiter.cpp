#include "configuration.h"

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "platform/w20_deckpro_shell/w20_lora_arbiter.h"
#include "platform/w20_deckpro/factory.h"
#include "platform/w20_deckpro/peripheral.h"
#include "platform/w20_deckpro/utilities.h"

#if defined(W20_USE_SEMTECH_RADIO)
#include "helpers/w20/W20SemtechRadio.h"
#include "target.h"
#endif

extern "C" {
#include "apps_common.h"
}

#ifdef __cplusplus
extern "C" {
#endif
bool ui_setting_get_lora_status(void);
#ifdef __cplusplus
}
#endif

enum LoraOwner { LORA_OWNER_NONE, LORA_OWNER_MESH, LORA_OWNER_DECK };
static LoraOwner s_target = LORA_OWNER_NONE;
static LoraOwner s_current = LORA_OWNER_NONE;

enum ArbState { ARB_IDLE, ARB_RELEASE_DECK, ARB_INIT_MESH, ARB_SHUTDOWN_MESH, ARB_INIT_DECK };
static ArbState s_state = ARB_IDLE;

static void w20_lora_detach_mesh_irq(void)
{
    lr20xx_set_mesh_dio_irq_cb(nullptr);
}

bool w20_mesh_radio_try_init(void)
{
#if defined(W20_USE_SEMTECH_RADIO)
    if (radio_driver.isReady())
        return true;
    return radio_driver.resumeAfterDeck();
#else
    return false;
#endif
}

void w20_mesh_radio_shutdown(void)
{
#if defined(W20_USE_SEMTECH_RADIO)
    radio_driver.shutdownForDeck();
#endif
    w20_lora_detach_mesh_irq();
}

bool w20_lora_hw_ok(void)
{
    if (peri_init_st[E_PERI_LORA])
        return true;
#if defined(W20_USE_SEMTECH_RADIO)
    return radio_driver.isReady();
#else
    return false;
#endif
}

void w20_lora_arbiter_request_mesh(void)
{
    s_target = LORA_OWNER_MESH;
#if defined(W20_USE_SEMTECH_RADIO)
    if (s_current == LORA_OWNER_MESH && radio_driver.isReady())
        return;
#endif
    if (s_current == LORA_OWNER_DECK)
        s_state = ARB_RELEASE_DECK;
    else
        s_state = ARB_INIT_MESH;
}

void w20_lora_arbiter_request_deck(void)
{
    if (!ui_setting_get_lora_status()) {
        Serial.println("W20: LoRa deck request ignored (module power OFF)");
        return;
    }
    s_target = LORA_OWNER_DECK;
    if (s_current == LORA_OWNER_DECK && peri_init_st[E_PERI_LORA])
        return;
    if (s_current == LORA_OWNER_DECK) {
        s_state = ARB_INIT_DECK;
#if defined(W20_USE_SEMTECH_RADIO)
    } else if (s_current == LORA_OWNER_MESH || radio_driver.isReady()) {
#else
    } else if (s_current == LORA_OWNER_MESH) {
#endif
        s_state = ARB_SHUTDOWN_MESH;
    } else {
        s_state = ARB_INIT_DECK;
    }
}

bool w20_lora_arbiter_ensure_deck(uint32_t timeout_ms)
{
    if (!ui_setting_get_lora_status()) {
        Serial.println("W20: ensure_deck aborted (module power OFF)");
        return false;
    }
    w20_lora_arbiter_request_deck();
    const uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        w20_lora_arbiter_tick();
        if (s_current == LORA_OWNER_DECK && peri_init_st[E_PERI_LORA])
            return true;
        delay(5);
    }
    return s_current == LORA_OWNER_DECK && peri_init_st[E_PERI_LORA];
}

void w20_lora_arbiter_tick(void)
{
    switch (s_state) {
    case ARB_IDLE:
        break;

    case ARB_RELEASE_DECK:
        lora_release_for_mesh();
        peri_init_st[E_PERI_LORA] = false;
        s_current = LORA_OWNER_NONE;
        s_state = ARB_INIT_MESH;
        break;

    case ARB_INIT_MESH:
        if (s_target == LORA_OWNER_MESH) {
            if (!w20_mesh_radio_try_init())
                Serial.println("W20: MeshCore Semtech radio init failed");
            s_current = LORA_OWNER_MESH;
        }
        s_state = ARB_IDLE;
        if (s_target == LORA_OWNER_DECK)
            s_state = ARB_SHUTDOWN_MESH;
        break;

    case ARB_SHUTDOWN_MESH:
        w20_mesh_radio_shutdown();
        delay(50);
        s_current = LORA_OWNER_NONE;
        s_state = ARB_INIT_DECK;
        break;

    case ARB_INIT_DECK:
        if (s_target == LORA_OWNER_DECK) {
            if (!ui_setting_get_lora_status()) {
                peri_init_st[E_PERI_LORA] = false;
                Serial.println("W20: Deck LoRa init skipped (module power OFF)");
            } else {
                peri_init_st[E_PERI_LORA] = lora_init();
                if (peri_init_st[E_PERI_LORA])
                    Serial.println("W20: Deck LoRa init OK (LR2021)");
                else
                    Serial.println("W20: Deck LoRa init failed");
            }
            s_current = LORA_OWNER_DECK;
        }
        s_state = ARB_IDLE;
        if (s_target == LORA_OWNER_MESH)
            s_state = ARB_RELEASE_DECK;
        break;

    default:
        break;
    }

#if defined(W20_USE_SEMTECH_RADIO)
    const bool mesh_ready = radio_driver.isReady();
#else
    const bool mesh_ready = false;
#endif

    if (s_state == ARB_IDLE && s_target != s_current) {
        if (s_target == LORA_OWNER_MESH) {
            s_state = (s_current == LORA_OWNER_DECK) ? ARB_RELEASE_DECK : ARB_INIT_MESH;
        } else if (s_target == LORA_OWNER_DECK) {
            s_state = (s_current == LORA_OWNER_MESH || mesh_ready) ? ARB_SHUTDOWN_MESH : ARB_INIT_DECK;
        }
    } else if (s_state == ARB_IDLE && s_target == LORA_OWNER_DECK && s_current == LORA_OWNER_DECK &&
               !peri_init_st[E_PERI_LORA] && ui_setting_get_lora_status()) {
        s_state = ARB_INIT_DECK;
    }
}

void w20_lora_mesh_drop_on_module_power_off(void)
{
    if (s_current == LORA_OWNER_DECK) {
        lora_release_for_mesh();
        peri_init_st[E_PERI_LORA] = false;
        s_current = LORA_OWNER_NONE;
    }
    w20_mesh_radio_shutdown();
    s_target = LORA_OWNER_MESH;
    s_state = ARB_IDLE;
    Serial.println("W20: LoRa module power OFF — radio drivers dropped");
}

bool w20_lora_mesh_reinit_after_module_power_on(uint32_t settle_ms, uint32_t timeout_ms)
{
    if (!ui_setting_get_lora_status())
        return false;

    if (settle_ms > 0)
        delay(settle_ms);

    if (s_current == LORA_OWNER_DECK) {
        lora_release_for_mesh();
        peri_init_st[E_PERI_LORA] = false;
        s_current = LORA_OWNER_NONE;
    }

    w20_mesh_radio_shutdown();

    s_target = LORA_OWNER_MESH;
    s_state = ARB_INIT_MESH;

    const uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        w20_lora_arbiter_tick();
#if defined(W20_USE_SEMTECH_RADIO)
        if (s_state == ARB_IDLE && s_current == LORA_OWNER_MESH && radio_driver.isReady()) {
            Serial.println("W20: MeshCore radio re-init after module power ON");
            return true;
        }
#endif
        delay(5);
    }

    Serial.println("W20: MeshCore radio re-init failed after module power ON");
#if defined(W20_USE_SEMTECH_RADIO)
    return radio_driver.isReady();
#else
    return false;
#endif
}

#endif /* W20_MESHNOLOGY && USE_W20_DECKPRO_SHELL */
