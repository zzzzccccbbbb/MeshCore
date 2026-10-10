#include "configuration.h"

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro/factory.h"
#include "platform/w20_deckpro/peripheral.h"
#include "platform/w20_deckpro/utilities.h"
#include "platform/w20_deckpro/w20_dbg_log.h"
#include "GPS.h"
#include "GPSStatus.h"

#ifdef __cplusplus
extern "C" {
#endif
bool ui_setting_get_gps_status(void);
#ifdef __cplusplus
}
#endif

extern meshtastic::GPSStatus *gpsStatus;

enum GpsOwner { GPS_OWNER_MESH, GPS_OWNER_DECK };
static GpsOwner s_target = GPS_OWNER_MESH;
static GpsOwner s_current = GPS_OWNER_MESH;

enum GpsArbState { GPS_ARB_IDLE, GPS_RELEASE_DECK, GPS_INIT_MESH, GPS_SHUTDOWN_MESH, GPS_INIT_DECK };
static GpsArbState s_state = GPS_ARB_IDLE;
static bool s_mesh_gps_handed_off = false;
static uint32_t s_deck_gps_init_after_ms = 0;

static const char *gps_arb_state_name(GpsArbState s)
{
    switch (s) {
    case GPS_ARB_IDLE:
        return "IDLE";
    case GPS_RELEASE_DECK:
        return "RELEASE_DECK";
    case GPS_INIT_MESH:
        return "INIT_MESH";
    case GPS_SHUTDOWN_MESH:
        return "SHUTDOWN_MESH";
    case GPS_INIT_DECK:
        return "INIT_DECK";
    default:
        return "?";
    }
}

static const char *gps_owner_name(GpsOwner o)
{
    return o == GPS_OWNER_DECK ? "DECK" : "MESH";
}

static void w20_mesh_gps_destroy(void)
{
#if !MESHTASTIC_EXCLUDE_GPS
    if (gps) {
        gps->handoffSuspendForDeck();
        delete gps;
        gps = nullptr;
    }
#endif
}

bool w20_mesh_gps_try_init(void)
{
#if MESHTASTIC_EXCLUDE_GPS
    return false;
#else
    if (!ui_setting_get_gps_status()) {
        return false;
    }
    if (!HAS_GPS)
        return false;
    if (config.position.gps_mode == meshtastic_Config_PositionConfig_GpsMode_NOT_PRESENT)
        return false;

    if (!gps) {
        gps = GPS::createGps();
        if (gps)
            gpsStatus->observe(&gps->newStatus);
    } else {
        gps->handoffResumeAfterDeck();
    }
    if (gps)
        LOG_INFO("W20: Meshtastic GPS ready");
    else
        LOG_WARN("W20: Meshtastic GPS init failed");
    s_mesh_gps_handed_off = false;
    return gps != nullptr;
#endif
}

void w20_mesh_gps_shutdown(void)
{
#if !MESHTASTIC_EXCLUDE_GPS
    if (gps && !s_mesh_gps_handed_off) {
        gps->handoffSuspendForDeck();
        s_mesh_gps_handed_off = true;
        LOG_INFO("W20: Meshtastic GPS released for Deck");
    }
#endif
}

bool w20_gps_hw_ok(void)
{
    if (peri_init_st[E_PERI_GPS])
        return true;
#if MESHTASTIC_EXCLUDE_GPS
    /* MeshCore：无 Meshtastic GPS 对象。Test PASS 跟 Setting Power GPS + EN 轨一致（同 LoRa 跟 isReady）。 */
    if (!ui_setting_get_gps_status()) {
        return false;
    }
#if (BOARD_GPS_EN >= 0)
    return digitalRead(BOARD_GPS_EN) == HIGH;
#else
    return true;
#endif
#else
    return gps != nullptr;
#endif
}

void w20_gps_arbiter_request_mesh(void)
{
    W20_GPS_LOGLN("[GPS-Arb] request_mesh");
    s_target = GPS_OWNER_MESH;
#if MESHTASTIC_EXCLUDE_GPS
    return;
#else
    if (s_current == GPS_OWNER_MESH && gps)
        return;
    if (s_current == GPS_OWNER_DECK)
        s_state = GPS_RELEASE_DECK;
    else
        s_state = GPS_INIT_MESH;
#endif
}

void w20_gps_arbiter_request_deck(void)
{
    if (!ui_setting_get_gps_status()) {
        LOG_WARN("W20: GPS deck request ignored (module power OFF)");
        W20_GPS_LOGLN("[GPS-Arb] request_deck ignored (module power OFF)");
        return;
    }
    W20_GPS_LOGLN("[GPS-Arb] request_deck");
    s_target = GPS_OWNER_DECK;
    /* 已属 Deck 且 init 成功：无需再抢 */
    if (s_current == GPS_OWNER_DECK && peri_init_st[E_PERI_GPS]) {
        return;
    }
    /* Mesh 已交出、Deck init 失败/未完成：只重试 INIT，勿再 SHUTDOWN（避免刷「released」） */
    if (s_current == GPS_OWNER_DECK || s_state == GPS_INIT_DECK) {
        if (s_state == GPS_ARB_IDLE || s_state == GPS_INIT_DECK) {
            s_state = GPS_INIT_DECK;
        }
        return;
    }
    if (s_state == GPS_SHUTDOWN_MESH || s_state == GPS_INIT_DECK) {
        return;
    }
    s_state = GPS_SHUTDOWN_MESH;
}

bool w20_gps_arbiter_ensure_deck(uint32_t timeout_ms)
{
    if (!ui_setting_get_gps_status()) {
        LOG_WARN("W20: ensure_deck aborted (module power OFF)");
        return false;
    }
    w20_gps_arbiter_request_deck();
    const uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        w20_gps_arbiter_tick();
        if (s_current == GPS_OWNER_DECK && peri_init_st[E_PERI_GPS])
            return true;
        delay(5);
    }
    return s_current == GPS_OWNER_DECK && peri_init_st[E_PERI_GPS];
}

void w20_gps_arbiter_tick(void)
{
    const GpsArbState state_in = s_state;
    if (state_in != GPS_ARB_IDLE) {
        W20_GPS_LOG("[GPS-Arb] tick %s target=%s current=%s peri=%d\r\n", gps_arb_state_name(state_in),
                    gps_owner_name(s_target), gps_owner_name(s_current), peri_init_st[E_PERI_GPS] ? 1 : 0);
    }

    switch (s_state) {
    case GPS_ARB_IDLE:
        break;

    case GPS_RELEASE_DECK:
        gps_release_for_mesh();
        peri_init_st[E_PERI_GPS] = false;
        s_state = GPS_INIT_MESH;
        break;

    case GPS_INIT_MESH:
        if (s_target == GPS_OWNER_MESH) {
            w20_mesh_gps_try_init();
            s_current = GPS_OWNER_MESH;
        }
        s_state = GPS_ARB_IDLE;
        if (s_target == GPS_OWNER_DECK)
            s_state = GPS_SHUTDOWN_MESH;
        break;

    case GPS_SHUTDOWN_MESH:
        w20_mesh_gps_shutdown();
        /* Mesh 已挂起后先标 DECK 归属，避免 IDLE 再因 target!=current 反复 SHUTDOWN */
        s_current = GPS_OWNER_DECK;
        /* UART end 后稍等再 begin，避免和 Mesh Serial.end 打架 */
        s_deck_gps_init_after_ms = millis() + 80;
        s_state = GPS_INIT_DECK;
        break;

    case GPS_INIT_DECK:
        if (s_target != GPS_OWNER_DECK) {
            s_state = GPS_ARB_IDLE;
            if (s_target == GPS_OWNER_MESH)
                s_state = GPS_RELEASE_DECK;
            break;
        }
#if USE_GPS
        if (!ui_setting_get_gps_status()) {
            peri_init_st[E_PERI_GPS] = false;
            LOG_WARN("W20: Deck GPS init skipped (module power OFF)");
            s_state = GPS_ARB_IDLE;
            break;
        }
        if (peri_init_st[E_PERI_GPS]) {
            s_current = GPS_OWNER_DECK;
            s_state = GPS_ARB_IDLE;
            break;
        }
        if (s_deck_gps_init_after_ms != 0 && (int32_t)(millis() - s_deck_gps_init_after_ms) < 0) {
            break;
        }
        peri_init_st[E_PERI_GPS] = gps_init();
        if (peri_init_st[E_PERI_GPS]) {
            LOG_INFO("W20: Deck GPS init OK");
            s_current = GPS_OWNER_DECK;
            s_state = GPS_ARB_IDLE;
            s_deck_gps_init_after_ms = 0;
        } else {
            LOG_WARN("W20: Deck GPS init failed — will retry");
            s_current = GPS_OWNER_DECK;
            s_deck_gps_init_after_ms = millis() + 200;
            /* 保持 GPS_INIT_DECK，下一次 tick 再试 */
        }
#else
        s_current = GPS_OWNER_DECK;
        s_state = GPS_ARB_IDLE;
#endif
        if (s_target == GPS_OWNER_MESH)
            s_state = GPS_RELEASE_DECK;
        break;

    default:
        break;
    }

    if (s_state == GPS_ARB_IDLE && s_target != s_current) {
        if (s_target == GPS_OWNER_MESH) {
            W20_GPS_LOGLN("[GPS-Arb] idle reconcile -> RELEASE_DECK");
            s_state = GPS_RELEASE_DECK;
        } else if (s_current == GPS_OWNER_DECK) {
            W20_GPS_LOGLN("[GPS-Arb] idle reconcile -> INIT_DECK");
            s_state = GPS_INIT_DECK;
        } else {
            W20_GPS_LOGLN("[GPS-Arb] idle reconcile -> SHUTDOWN_MESH");
            s_state = GPS_SHUTDOWN_MESH;
        }
    }
}

void w20_gps_mesh_drop_on_module_power_off(void)
{
    if (s_current == GPS_OWNER_DECK) {
        gps_release_for_mesh();
        peri_init_st[E_PERI_GPS] = false;
        s_current = GPS_OWNER_MESH;
    }
    w20_mesh_gps_destroy();
    s_mesh_gps_handed_off = false;
    s_target = GPS_OWNER_MESH;
    s_state = GPS_ARB_IDLE;
    LOG_INFO("W20: GPS module power OFF — Mesh/Deck GPS drivers dropped");
}

bool w20_gps_mesh_reinit_after_module_power_on(uint32_t settle_ms, uint32_t timeout_ms)
{
    if (!ui_setting_get_gps_status()) {
        return false;
    }

    if (settle_ms > 0) {
        delay(settle_ms);
    }

    if (s_current == GPS_OWNER_DECK) {
        gps_release_for_mesh();
        peri_init_st[E_PERI_GPS] = false;
        s_current = GPS_OWNER_MESH;
    }

    w20_mesh_gps_destroy();
    s_mesh_gps_handed_off = false;
    s_target = GPS_OWNER_MESH;

#if MESHTASTIC_EXCLUDE_GPS || !HAS_GPS
    /* MeshCore：无 Meshtastic GPS 对象；上电只恢复轨/仲裁，进 GPS 页再 request_deck。
     * 旧逻辑等 gps!=nullptr 必超时 ~3s，阻塞 Setting 点击且常被第二次 CLICKED 又关掉。 */
    (void)timeout_ms;
    s_current = GPS_OWNER_MESH;
    s_state = GPS_ARB_IDLE;
    LOG_INFO("W20: GPS module power ON (rail only, MeshCore)");
    return true;
#else
    s_state = GPS_INIT_MESH;

    const uint32_t t0 = millis();
    while (millis() - t0 < timeout_ms) {
        w20_gps_arbiter_tick();
        if (s_state == GPS_ARB_IDLE && s_current == GPS_OWNER_MESH && gps != nullptr) {
            LOG_INFO("W20: Meshtastic GPS re-init after module power ON");
            return true;
        }
        delay(5);
    }

    LOG_WARN("W20: Meshtastic GPS re-init failed after module power ON");
    return gps != nullptr;
#endif
}

#endif
