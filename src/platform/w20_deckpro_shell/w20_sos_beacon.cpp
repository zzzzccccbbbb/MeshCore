/**
 * SOS：长按照明键进入闪烁后，周期性向自己建的 GroupChannel 发一条（跳过 Public）。
 * 有定位（MeshCore GPS 或 Deck GPS）则带经纬度，否则只发 SOS。
 * 频道里收到别人的消息则停发并关灯。
 */
#include "configuration.h"
#include "platform/w20_deckpro_shell/w20_sos_beacon.h"

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "helpers/ChannelDetails.h"
#include "../../../examples/companion_radio/MyMesh.h"
#include "peri_lighting_led_key.h"
#include "platform/w20_deckpro/peripheral.h"
#include "platform/w20_deckpro/ui_deckpro_port.h"
#include "platform/w20_deckpro/w20_dbg_log.h"

/* SF12 airtime ~1.7s; 3s interval left almost no RX window → peer reply often missed. */
#ifndef W20_SOS_BEACON_INTERVAL_MS
#define W20_SOS_BEACON_INTERVAL_MS 10000u
#endif

static bool s_active = false;
static uint32_t s_last_send_ms = 0;

/** true if text is "ourName: ..." (own flood echo / local loop). */
static bool sos_text_is_own(const char *text)
{
    if (!text || !text[0]) {
        return false;
    }
    const char *name = "W20";
    NodePrefs *prefs = the_mesh.getNodePrefs();
    if (prefs && prefs->node_name[0]) {
        name = prefs->node_name;
    }
    const size_t n = strlen(name);
    if (n == 0) {
        return false;
    }
    /* wire format: "<sender_name>: <msg>" */
    if (strncmp(text, name, n) != 0 || text[n] != ':') {
        return false;
    }
    return true;
}

static bool sos_fix(double *lat, double *lon)
{
#if ENV_INCLUDE_GPS == 1
    LocationProvider *nmea = sensors.getLocationProvider();
    if (nmea && nmea->isValid()) {
        *lat = (double)nmea->getLatitude() / 1000000.0;
        *lon = (double)nmea->getLongitude() / 1000000.0;
        return true;
    }
#endif
    if (gps_get_fix_valid()) {
        gps_get_coord(lat, lon);
        return true;
    }
    return false;
}

static bool sos_send_once(void)
{
    ChannelDetails ch;
    int idx = -1;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
        if (!the_mesh.getChannel(i, ch)) {
            break;
        }
        if (ch.name[0] == 0 || strcmp(ch.name, "Public") == 0) {
            continue;
        }
        idx = i;
        break;
    }
    if (idx < 0) {
        W20_SOS_LOGLN("[SOS] skip: no channel");
        return false;
    }

    double lat = 0;
    double lon = 0;
    char text[64];
    if (sos_fix(&lat, &lon)) {
        snprintf(text, sizeof(text), "SOS %.5f,%.5f", lat, lon);
    } else {
        snprintf(text, sizeof(text), "SOS");
    }

    const char *name = "W20";
    NodePrefs *prefs = the_mesh.getNodePrefs();
    if (prefs && prefs->node_name[0]) {
        name = prefs->node_name;
    }
    const uint32_t ts = (uint32_t)rtc_clock.getCurrentTime();
    const bool ok = the_mesh.sendGroupMessage(ts, ch.channel, name, text, (int)strlen(text));
    W20_SOS_LOG("[SOS] %s ch=%s %s\n", ok ? "sent" : "send fail", ch.name, text);
    if (ok) {
        char shown[96];
        snprintf(shown, sizeof(shown), "%s: %s", name, text);
        the_mesh.pushLocalChannelMessage((uint8_t)idx, ts, shown);
    }
    return ok;
}

void w20_sos_beacon_start(void)
{
    s_active = true;
    s_last_send_ms = 0;
    W20_SOS_LOGLN("[SOS] beacon start");
}

void w20_sos_beacon_stop(void)
{
    if (!s_active) {
        return;
    }
    s_active = false;
    s_last_send_ms = 0;
    W20_SOS_LOGLN("[SOS] beacon stop");
}

void w20_sos_beacon_poll(void)
{
    if (!s_active) {
        return;
    }
    const uint32_t now = millis();
    if (s_last_send_ms != 0 && (uint32_t)(now - s_last_send_ms) < W20_SOS_BEACON_INTERVAL_MS) {
        return;
    }
    if (sos_send_once()) {
        s_last_send_ms = now;
    }
}

bool w20_sos_beacon_active(void)
{
    return s_active;
}

void w20_sos_on_channel_text(const char *text)
{
    if (!s_active || text == nullptr) {
        return;
    }
    /* 自己的 "name: ..." 回环不算别人回复（任意正文，不限 SOS） */
    if (sos_text_is_own(text)) {
        W20_SOS_LOGLN("[SOS] ignore own channel echo");
        return;
    }
    W20_SOS_LOG("[SOS] channel reply -> stop text=%s\n", text);
    peri_lighting_led_key_sync_from_ui(false);
}

void w20_sos_on_peer_message(void)
{
    if (!s_active) {
        return;
    }
    W20_SOS_LOGLN("[SOS] peer DM/msg -> stop");
    peri_lighting_led_key_sync_from_ui(false);
}

#else

void w20_sos_beacon_start(void) {}
void w20_sos_beacon_stop(void) {}
void w20_sos_beacon_poll(void) {}
bool w20_sos_beacon_active(void)
{
    return false;
}
void w20_sos_on_channel_text(const char *) {}
void w20_sos_on_peer_message(void) {}

#endif
