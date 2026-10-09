/************************************************************************
 * W20 Deck Pro — WiFi captive-portal provisioning
 ************************************************************************/
#if defined(ARDUINO_ARCH_ESP32)

#include "ui_wifi_provision.h"
#include "w20_dbg_log.h"
#include "ui_deckpro_port.h"
#include "target_specific.h"
#include "mesh/NodeDB.h"
#include "mesh/wifi/WiFiAPClient.h"
#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "peripheral.h"
#endif

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "w20_wifi_ble_coex.h"
#if defined(BLE_PIN_CODE)
#include <helpers/esp32/SerialBLEInterface.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#if defined(ARCH_ESP32) && !MESHTASTIC_EXCLUDE_BLUETOOTH && HAS_BLUETOOTH
#include "main.h"
#endif

static DNSServer s_dns;
static WebServer s_http(80);
static bool s_running = false;
static bool s_dnsRunning = false;
static bool s_routesRegistered = false;
static volatile bool s_taskRun = false;
static TaskHandle_t s_taskHandle = nullptr;
static ui_wifi_prov_state_t s_state = UI_WIFI_PROV_IDLE;
static char s_apSsid[20] = {0};
static unsigned long s_connectStartMs = 0;
static bool s_connectPending = false;
static char s_pendingSsid[33] = {0};
static char s_pendingPass[65] = {0};
static String s_scanJsonCache = "{\"status\":\"ok\",\"scanning\":false,\"data\":[]}";
static unsigned long s_lastScanMs = 0;
static bool s_scanInProgress = false;
static unsigned long s_apWatchLastMs = 0;
static volatile bool s_startRequested = false;
static volatile bool s_stopRequested = false;
/** start() arrived while stop in progress — restart SoftAP after teardown. */
static volatile bool s_pendingStartAfterStop = false;
static bool s_keepStaOnStart = false;
static bool s_softapUp = false;
/** softAP 刚起来时暂缓 TX/bandwidth boost，避免立刻 stop/start 抖掉手机。 */
static unsigned long s_softapBoostDueMs = 0;
static bool s_softapBoostPending = false;
/** 用户点 Disconnect 后保持断网，直到配网成功；开机默认 false。 */
static bool s_userHoldOffline = false;
static ui_wifi_prov_ui_cb_t s_uiCb = nullptr;
static uint32_t s_uiNotifyFp = UINT32_MAX;
static unsigned long s_connectLastLogMs = 0;
static uint8_t s_lastLoggedDiscReason = 255;
static unsigned long s_connectedPortalHoldStart = 0;
static bool s_connectFailed = false;
static bool s_staConnectOk = false;
static uint8_t s_connectedStatusServed = 0;
static char s_staConnectIp[16] = {0};
static const char PROV_CONNECT_FAIL_MSG[] = "Connection failed. Retry!";
static constexpr unsigned long kPortalHoldMinMs = 2000; /* 拿到 IP 后最短再留 SoftAP，供手机收 status */
static constexpr unsigned long kPortalHoldAfterPollMs = 1500; /* 已轮询到 connected 后再留窗口再拆 */
static constexpr unsigned long kPortalHoldMaxMs = 25000; /* 等手机轮询 connected；扫描曾踢掉手机时需更长 */
static constexpr unsigned long kPortalHoldAbsMaxMs = 45000; /* 有客户端仍未轮询到时的硬上限 */
static constexpr uint8_t kPortalHoldMinStatusPolls = 1;  /* 至少一次 status 返回 connected */
/** STA 卡在 DISCONNECTED+reason=0 时重试 WiFi.begin 的间隔与次数（不含首次）。 */
static constexpr unsigned long kStaBeginRetryIntervalMs = 12000;
static constexpr uint8_t kStaBeginMaxExtraRetries = 2;
static uint8_t s_staBeginRetryCount = 0;
static unsigned long s_lastStaBeginMs = 0;
/** SoftAP 因有手机客户端而暂缓对齐到目标路由器信道；客户端离开后再 realign。 */
static int s_deferredAlignChannel = -1;
/** AUTH_EXPIRE / 握手超时等：跨信道 AP+STA 易失败，有限次重试。 */
static constexpr unsigned long kAuthRetryIntervalMs = 2500;
static constexpr uint8_t kAuthRetryMax = 3;
static uint8_t s_authRetryCount = 0;
static unsigned long s_lastAuthRetryMs = 0;

static bool staHasValidIp(void)
{
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

static void resetConnectProgressFlags(void)
{
    s_staConnectOk = false;
    s_connectedStatusServed = 0;
    s_connectedPortalHoldStart = 0;
    s_staConnectIp[0] = '\0';
    s_staBeginRetryCount = 0;
    s_lastStaBeginMs = 0;
    s_deferredAlignChannel = -1;
    s_authRetryCount = 0;
    s_lastAuthRetryMs = 0;
}

static const char *wifiStaStatusStr(wl_status_t s)
{
    switch (s) {
    case WL_IDLE_STATUS:
        return "IDLE";
    case WL_NO_SSID_AVAIL:
        return "NO_SSID";
    case WL_SCAN_COMPLETED:
        return "SCAN_DONE";
    case WL_CONNECTED:
        return "CONNECTED";
    case WL_CONNECT_FAILED:
        return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
        return "LOST";
    case WL_DISCONNECTED:
        return "DISCONNECTED";
    default:
        return "?";
    }
}

static const char *wifiReasonStr(uint8_t reason)
{
    switch (reason) {
    case 1:
        return "UNSPECIFIED";
    case 2:
        return "AUTH_EXPIRE";
    case 3:
        return "AUTH_LEAVE";
    case 4:
        return "ASSOC_EXPIRE";
    case 5:
        return "ASSOC_TOOMANY";
    case 15:
        return "4WAY_HANDSHAKE_TIMEOUT";
    case 39:
        return "TIMEOUT";
    case 201:
        return "NO_AP_FOUND";
    case 202:
        return "AUTH_FAIL";
    case 204:
        return "HANDSHAKE_FAIL";
    case 205:
        return "CONN_FAIL";
    default: {
        static char buf[20];
        snprintf(buf, sizeof(buf), "CODE_%u", reason);
        return buf;
    }
    }
}

static void provLogStaSnapshot(const char *tag)
{
    W20_WIFI_LOG("[WiFiProv] %s mode=%d sta=%s(%d) ip=%s disc_reason=%u(%s) autoReconnect=%d\n", tag, (int)WiFi.getMode(),
                  wifiStaStatusStr(WiFi.status()), (int)WiFi.status(), WiFi.localIP().toString().c_str(),
                  (unsigned)getWifiDisconnectReason(), wifiReasonStr(getWifiDisconnectReason()),
                  WiFi.getAutoReconnect() ? 1 : 0);
}

/** applyPendingConnect 前：若 pending 密码为空且 SSID 与 NVS 相同则沿用已存 PSK（网页 POST 已要求非空密码）。 */
static void resolvePendingCredentials(void)
{
    if (s_pendingPass[0] != '\0')
        return;
    if (strcmp(s_pendingSsid, config.network.wifi_ssid) != 0)
        return;
    if (config.network.wifi_psk[0] == '\0')
        return;
    strncpy(s_pendingPass, config.network.wifi_psk, sizeof(s_pendingPass) - 1);
    s_pendingPass[sizeof(s_pendingPass) - 1] = '\0';
    W20_WIFI_LOG("[WiFiProv] connect: reuse saved PSK (len=%u)\n", (unsigned)strlen(s_pendingPass));
}

static uint32_t provUiFingerprint(void)
{
    uint32_t fp = (uint32_t)s_state;
    /* 不把 softAPgetStationNum 计入指纹：手机 join 抖动会狂刷 UI → 墨水 SPI，门户易卡死 */
    fp ^= s_connectFailed ? 0x80000000u : 0u;
    if (s_state == UI_WIFI_PROV_CONNECTING || s_state == UI_WIFI_PROV_CONNECTED) {
        fp ^= ((uint32_t)WiFi.status() << 16);
        fp ^= (uint32_t)WiFi.localIP();
    }
    return fp;
}

static void provNotifyUiIfChanged(void)
{
    uint32_t fp = provUiFingerprint();
    if (fp == s_uiNotifyFp)
        return;
    s_uiNotifyFp = fp;
    if (s_uiCb) {
        s_uiCb();
    }
}

void ui_wifi_provisioning_set_ui_callback(ui_wifi_prov_ui_cb_t cb)
{
    s_uiCb = cb;
    s_uiNotifyFp = UINT32_MAX;
    if (cb) {
        provNotifyUiIfChanged();
    }
}

bool ui_wifi_provisioning_active(void)
{
    return s_running || s_startRequested || s_state == UI_WIFI_PROV_STARTING || s_state == UI_WIFI_PROV_AP;
}

bool ui_wifi_provisioning_in_sta_handoff(void)
{
    return s_state == UI_WIFI_PROV_CONNECTING || s_state == UI_WIFI_PROV_CONNECTED;
}

static void jsonEscape(const char *in, char *out, size_t outLen)
{
    if (!out || outLen == 0)
        return;
    if (!in) {
        out[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j + 2 < outLen; i++) {
        char c = in[i];
        if (c == '"' || c == '\\') {
            if (j + 2 >= outLen)
                break;
            out[j++] = '\\';
        }
        out[j++] = c;
    }
    out[j] = '\0';
}

static bool extractJsonString(const char *body, const char *key, char *out, size_t outLen)
{
    if (!body || !key || !out || outLen == 0)
        return false;
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(body, pattern);
    if (!p)
        return false;
    p = strchr(p + strlen(pattern), '"');
    if (!p)
        return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outLen) {
        if (*p == '\\' && p[1]) {
            p++;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

static void handleRoot();
static void sendPortalRedirect();
static bool queueStaConnectFromCredentials(const char *ssid, const char *pass, String *errOut);
static void rebuildScanJsonFromSsidCache(void);

static void sendCaptivePortalPage()
{
    /* 系统登录小窗常拦 302；探测 URL 直接下发门户，与 GET / 同页 */
    handleRoot();
}

static void sendPortalRedirect()
{
    /* Android generate_204：非 204 即判定需登录；直接下发门户比 302 更稳（Private DNS 外也可用） */
    W20_WIFI_LOGLN("[WiFiProv] captive probe -> portal page");
    handleRoot();
}

/* SoftAP 门户：纯 PROGMEM 静态页（无堆上拼 6KB String），SSID 由 JS 拉 /json/scanNetworks。
 * WiFiClient 按值拷贝写正文会失败（send_ms~10s ok=0）；send_P 走 WebServer 内部 client。 */
static const char PORTAL_PAGE[] PROGMEM =
    "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>w20 WiFi</title><style>"
    ":root{--bg:#0c1117;--card:#151d28;--t:#e6edf5;--m:#8b9cb3;--a:#3dd68c;--b:#263244;--e:#ff7b7b}"
    "*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--t);padding:16px}"
    ".w{max-width:420px;margin:0 auto}.h{display:flex;gap:10px;align-items:center;margin:0 0 14px}"
    ".logo{width:36px;height:36px;border-radius:10px;background:var(--a);display:flex;align-items:center;"
    "justify-content:center;font-size:9px;font-weight:800;color:#0c1117}"
    "h1{margin:0;font-size:15px}p.s{margin:2px 0 0;font-size:12px;color:var(--m)}"
    ".c{background:var(--card);border:1px solid var(--b);border-radius:14px;padding:14px;margin:0 0 10px}"
    ".l{display:block;font-size:12px;color:var(--m);margin:0 0 4px}.r{display:flex;gap:8px}"
    ".r select,.r input{flex:1;min-width:0;margin:0}"
    "select,input,button{width:100%;padding:11px 12px;margin:0 0 8px;border-radius:9px;border:1px solid var(--b);"
    "background:#0f1620;color:var(--t);font-size:15px}"
    "select:focus,input:focus{outline:none;border-color:var(--a);"
    "box-shadow:0 0 0 2px rgba(61,214,140,.35);caret-color:var(--a)}"
    "select:focus-visible,input:focus-visible{outline:none}"
    "select option:checked,select option:focus{background:var(--a);color:#0c1117}"
    "button{margin:2px 0 0;background:var(--a);color:#0c1117;border:0;font-weight:600}"
    "button:disabled{opacity:.45}.bs{background:transparent;color:var(--m);border:1px solid var(--b);width:auto;flex-shrink:0;margin:0}"
    ".be{width:44px;min-width:44px;padding:0;display:flex;align-items:center;justify-content:center}"
    ".be.on{color:var(--a);border-color:rgba(61,214,140,.45)}"
    ".ey{width:18px;height:11px;border:2px solid currentColor;border-radius:50%;position:relative;display:block}"
    ".ey:after{content:\"\";position:absolute;left:50%;top:50%;width:5px;height:5px;margin:-2.5px 0 0 -2.5px;"
    "background:currentColor;border-radius:50%}.be.on .ey{opacity:.35}"
    ".st{min-height:40px;padding:10px 12px;border-radius:9px;background:#0f1620;border:1px solid var(--b);font-size:13px}"
    ".ok{border-color:rgba(61,214,140,.35);color:var(--a)}.er{border-color:rgba(255,123,123,.35);color:var(--e)}"
    ".hi{font-size:12px;color:var(--m);margin:0 0 10px;line-height:1.4}"
    "</style></head><body><div class=w><div class=h><div class=logo>W20</div>"
    "<div><h1>w20 MESHnology</h1><p class=s>WiFi Setup</p></div></div><div class=c>"
    "<p class=hi>Connect to your router or phone hotspot.</p>"
    "<label class=l>Network</label><div class=r>"
    "<select id=ssid><option value=\"\">Loading...</option></select>"
    "<button type=button class=bs id=btnScan onclick=scan()>Refresh</button></div>"
    "<label class=l>Password</label><div class=r>"
    "<input id=pass type=password placeholder=\"WiFi password\" autocomplete=off>"
    "<button type=button class=\"bs be\" id=btnEye onclick=togglePass() title=\"Show password\" aria-label=\"Show password\">"
    "<span class=ey></span></button></div>"
    "<button type=button id=btnConn onclick=connect()>Connect Network</button></div>"
    "<div id=msg class=st>Loading networks...</div></div><script>"
    "var T=null,S=null,busy=0,ok=0,pf=0,pa=0;"
    "function msg(t,c){var m=document.getElementById('msg');m.className='st '+(c||'');m.textContent=t;}"
    "function busySet(v){busy=v&&!ok;document.getElementById('btnConn').disabled=busy||ok;}"
    "function done(){ok=1;busy=1;var b=document.getElementById('btnConn');b.disabled=1;b.textContent='Connected';}"
    "function togglePass(){var p=document.getElementById('pass'),b=document.getElementById('btnEye'),show=p.type==='password';"
    "p.type=show?'text':'password';b.classList.toggle('on',show);"
    "b.title=show?'Hide password':'Show password';b.setAttribute('aria-label',b.title);}"
    "function scan(){if(T)clearTimeout(T);document.getElementById('btnScan').disabled=1;"
    "if(!ok)msg('Refreshing...');"
    "fetch('/json/scanNetworks').then(function(r){return r.json();}).then(function(j){"
    "var s=document.getElementById('ssid'),L=j.data||[];"
    "if(j.scanning){if(!ok)msg('Scanning...');if(!L.length)s.innerHTML='<option value=\"\">Scanning...</option>';"
    "else{s.innerHTML='';L.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;"
    "o.textContent=n.ssid+' ('+n.rssi+' dBm)';s.appendChild(o);});}T=setTimeout(scan,2000);return;}"
    "document.getElementById('btnScan').disabled=0;s.innerHTML='';"
    "if(!L.length){s.innerHTML='<option value=\"\">(no networks)</option>';if(!ok)msg('No networks. Tap Refresh.','er');return;}"
    "L.forEach(function(n){var o=document.createElement('option');o.value=n.ssid;"
    "o.textContent=n.ssid+' ('+n.rssi+' dBm)';s.appendChild(o);});"
    "if(!busy&&!ok)msg('Select a network and enter password');"
    "}).catch(function(){document.getElementById('btnScan').disabled=0;if(!ok)msg('Scan failed','er');});}"
    "function poll(){if(S)clearTimeout(S);pa++;"
    "fetch('/json/wifi/status').then(function(r){return r.json();}).then(function(j){pf=0;"
    "if(j.connected){done();msg('Connected'+(j.ip?(' — '+j.ip):''),'ok');return;}"
    "if(j.failed){busySet(0);msg(j.message||'Connection failed','er');return;}"
    "msg(j.message||'Connecting...');S=setTimeout(poll,500);})"
    ".catch(function(){pf++;if(pa>=40&&pf>=20){done();msg('Connected on device','ok');return;}"
    "msg('Waiting...');S=setTimeout(poll,800);});}"
    "function connect(){if(ok)return;var a=document.getElementById('ssid').value,p=document.getElementById('pass').value;"
    "if(!a){msg('Select a network','er');return;}if(!p){msg('Password required','er');return;}"
    "if(p.length<8){msg('Connection failed','er');return;}busySet(1);pf=0;pa=0;msg('Submitting...');"
    "fetch('/json/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify({ssid:a,password:p})}).then(function(r){return r.json();}).then(function(j){"
    "if(j.status==='ok')poll();else{busySet(0);msg(j.message||'Submit failed','er');}})"
    ".catch(function(){busySet(0);msg('Request failed','er');});}"
    "document.getElementById('ssid').onchange=function(){if(!ok)document.getElementById('pass').value='';};"
    "scan();"
    "</script></body></html>";

/** SoftAP 下用 sendContent 按块写出 PROGMEM，走 WebServer 内部 client（勿拷贝 WiFiClient 写正文）。 */
static bool portalSendProgmem(PGM_P page)
{
    const size_t len = strlen_P(page);
    if (len == 0) {
        return false;
    }
    /* 拷贝仅用来缩短底层 socket 超时；正文仍走 sendContent/_currentClient */
    {
        WiFiClient c = s_http.client();
        c.setTimeout(1500);
    }
    s_http.setContentLength(len);
    s_http.sendHeader("Cache-Control", "no-cache");
    s_http.sendHeader("Access-Control-Allow-Origin", "*");
    s_http.sendHeader("Connection", "close");
    s_http.send(200, "text/html", "");

    char buf[256];
    size_t off = 0;
    while (off < len) {
        size_t n = len - off;
        if (n > sizeof(buf)) {
            n = sizeof(buf);
        }
        memcpy_P(buf, page + off, n);
        s_http.sendContent(buf, n);
        off += n;
        delay(0);
    }
    return true;
}

static void handleRoot()
{
    const uint32_t t0 = millis();
    W20_WIFI_LOGLN("[WiFiProv] HTTP GET /");
    /* 选信道扫描已有 SSID 缓存，先重建 JSON，供页内 scan() 秒回 */
    rebuildScanJsonFromSsidCache();
    const bool ok = portalSendProgmem(PORTAL_PAGE);
    W20_WIFI_LOG("[WiFiProv] GET / send_P len=%u send_ms=%lu ok=%d\n", (unsigned)strlen_P(PORTAL_PAGE),
                  (unsigned long)(millis() - t0), ok ? 1 : 0);
}

static constexpr unsigned long kScanCacheMinMs = 20000;      /* 缓存有效期内不重复扫 */
static constexpr unsigned long kScanDeferWithClientMs = 120000; /* 有手机连 AP 时最长用旧缓存 */

static bool ensureSoftApPortalUp(const char *reason);
static bool startSoftApPortal(const IPAddress &apIp, const char *ssid, bool scan_for_channel);
static void finishAsyncScanIfReady(void);
static bool startAsyncScan(void);
static void rememberSsidChannelsFromScan(int apCount);
static int lookupCachedSsidChannel(const char *ssid);

static String buildScanDataArrayJson(int apCount)
{
    ui_wifi_scan_entry_t entries[32];
    const int n = ui_wifi_dedup_scan_results(entries, 32, apCount, true, false);

    String data = "[";
    char esc[64];
    for (int i = 0; i < n; i++) {
        if (i > 0)
            data += ',';
        jsonEscape(entries[i].ssid, esc, sizeof(esc));
        data += "{\"ssid\":\"";
        data += esc;
        data += "\",\"rssi\":";
        data += String(entries[i].rssi);
        data += '}';
    }
    data += "]";
    return data;
}

static void setScanJsonCache(int apCount, bool scanning)
{
    String json = "{\"status\":\"ok\",\"scanning\":";
    json += scanning ? "true" : "false";
    json += ",\"data\":";
    json += buildScanDataArrayJson(apCount);
    json += "}";
    s_scanJsonCache = json;
}

static bool startAsyncScan(void)
{
    if (s_scanInProgress)
        return false;
    /* 有手机连 SoftAP 时禁止 scan，否则单射频会掐门户 */
    if (WiFi.softAPgetStationNum() > 0) {
        W20_WIFI_LOGLN("[WiFiProv] async scan blocked (SoftAP has client)");
        return false;
    }

    int ret = WiFi.scanNetworks(true, true);
    if (ret == WIFI_SCAN_FAILED) {
        Serial.println("[WiFiProv] async scan start failed");
        return false;
    }

    s_scanInProgress = true;
    /* 扫描中保留旧列表，避免门户短暂空列表；仅改 scanning 标志 */
    if (s_scanJsonCache.indexOf("\"ssid\"") >= 0) {
        s_scanJsonCache.replace("\"scanning\":false", "\"scanning\":true");
    } else {
        setScanJsonCache(0, true);
        rebuildScanJsonFromSsidCache();
        if (s_scanJsonCache.indexOf("\"ssid\"") >= 0) {
            s_scanJsonCache.replace("\"scanning\":false", "\"scanning\":true");
        }
    }
    W20_WIFI_LOGLN("[WiFiProv] WiFi async scan start (HTTP returns immediately)");
    return true;
}

static void finishAsyncScanIfReady(void)
{
    if (!s_scanInProgress)
        return;

    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING)
        return;

    s_scanInProgress = false;
    if (n == WIFI_SCAN_FAILED) {
        Serial.println("[WiFiProv] async scan failed");
        ensureSoftApPortalUp("scan_failed");
        return;
    }

    setScanJsonCache(n, false);
    s_lastScanMs = millis();
    rememberSsidChannelsFromScan(n);
    WiFi.scanDelete();
    W20_WIFI_LOG("[WiFiProv] WiFi async scan done, %d APs\n", n);
    ensureSoftApPortalUp("after_scan");
}

/** @return true if cache was updated or async scan started */
static bool refreshScanCache(bool force)
{
    finishAsyncScanIfReady();
    if (s_scanInProgress)
        return false;

    const unsigned long now = millis();
    const unsigned long age = (s_lastScanMs == 0) ? ULONG_MAX : (now - s_lastScanMs);
    const int clients = WiFi.softAPgetStationNum();
    const bool cacheHasSsids = (s_scanJsonCache.indexOf("\"ssid\"") >= 0);

    /*
     * SoftAP 有手机关联时禁止启动新 scan：ESP32 单射频扫网会停 SoftAP，门户立刻掉线。
     * 有缓存就直接复用（含选信道时预填的列表）；仅空缓存才允许扫一次。
     */
    if (clients > 0) {
        /* 有手机时绝不再 scan（会掐 SoftAP）；列表靠选信道缓存 + 手输 SSID */
        W20_WIFI_LOG("[WiFiProv] scan skipped (%d client(s), cache_ssids=%d age=%lus)\n", clients,
                      cacheHasSsids ? 1 : 0, age == ULONG_MAX ? 0UL : age / 1000);
        return false;
    }

    if (!force) {
        if (age < kScanCacheMinMs)
            return false;
        if (age < kScanDeferWithClientMs && cacheHasSsids) {
            return false;
        }
    } else if (age < kScanCacheMinMs && cacheHasSsids) {
        W20_WIFI_LOG("[WiFiProv] scan skipped on request (cache fresh age=%lus)\n", age / 1000);
        return false;
    }

    s_scanInProgress = true;
    W20_WIFI_LOGLN("[WiFiProv] WiFi sync scan start (no AP clients)");
    const int n = WiFi.scanNetworks(false, true);
    rememberSsidChannelsFromScan(n);
    setScanJsonCache(n, false);
    s_lastScanMs = now;
    s_scanInProgress = false;
    W20_WIFI_LOG("[WiFiProv] WiFi sync scan done, %d APs\n", n);
    ensureSoftApPortalUp("after_scan");
    return true;
}

/** ESP32 在 AP_STA 下 scan 会关掉 SoftAP；扫描后或周期检查并恢复 beacon。 */
static bool ensureSoftApPortalUp(const char *reason)
{
    if (!s_running || s_state == UI_WIFI_PROV_CONNECTING || s_apSsid[0] == '\0')
        return false;

    const IPAddress apIp(192, 168, 4, 1);
    wifi_mode_t mode = WiFi.getMode();
    /* 配网门户（未提交连路由器）：禁止 AP_STA，否则 STA 空连/重连会搞垮门户 */
    if (s_state == UI_WIFI_PROV_AP && mode == WIFI_AP_STA) {
        /* 已有手机连 SoftAP：勿 demote/restart，会立刻踢客户端、门户 40s 卡死 */
        if (WiFi.softAPgetStationNum() > 0) {
            W20_WIFI_LOG("[WiFiProv] AP watch skip demote (clients>0, %s)\n", reason);
            return true;
        }
        W20_WIFI_LOG("[WiFiProv] AP watch demote AP_STA→AP (%s)\n", reason);
        WiFi.disconnect(true, true);
        delay(50);
        WiFi.mode(WIFI_AP);
        delay(80);
        mode = WiFi.getMode();
    }
    const bool modeOk = (mode == WIFI_AP || (s_state == UI_WIFI_PROV_CONNECTING && mode == WIFI_AP_STA));
    const bool ipOk = (WiFi.softAPIP() == apIp);
    const bool ssidOk = (WiFi.softAPSSID() == s_apSsid);

    if (modeOk && ipOk && ssidOk)
        return true;

    /* 手机已连上时禁止 recover softAP（会 WIFI_OFF 再起，HTTP/DNS 全断） */
    if (WiFi.softAPgetStationNum() > 0) {
        W20_WIFI_LOG("[WiFiProv] AP recover skipped (clients>0, %s) mode=%d\n", reason, (int)mode);
        return false;
    }

    W20_WIFI_LOG("[WiFiProv] AP recover (%s) mode=%d ip=%s ssid=%s expect=%s\n", reason, (int)mode,
                  WiFi.softAPIP().toString().c_str(), WiFi.softAPSSID().c_str(), s_apSsid);
    if (!startSoftApPortal(apIp, s_apSsid, false)) {
        Serial.println("[WiFiProv] AP recover failed");
        return false;
    }
    if (!s_dnsRunning) {
        s_dns.start(53, "*", apIp);
        s_dnsRunning = true;
    }
    return true;
}

static void handleScanNetworks()
{
    W20_WIFI_LOGLN("[WiFiProv] HTTP GET /json/scanNetworks");
    finishAsyncScanIfReady();

    /* 优先用选信道时留下的 SSID 缓存秒回，勿在手机已连 SoftAP 时再扫 */
    rebuildScanJsonFromSsidCache();

    const int clients = WiFi.softAPgetStationNum();
    const unsigned long age = (s_lastScanMs == 0) ? ULONG_MAX : (millis() - s_lastScanMs);
    const bool cacheEmpty = (s_scanJsonCache.indexOf("\"ssid\"") < 0);

    /* 门户运行中禁止同步扫网：clients 短暂为 0 时 sync scan 会掐掉 SoftAP beacon → 手机无网络/无弹窗 */
    if (s_running) {
        W20_WIFI_LOG("[WiFiProv] scanNetworks serve-cache only (portal clients=%d len=%u)\n", clients,
                      (unsigned)s_scanJsonCache.length());
    } else if (clients == 0 && (cacheEmpty || age >= kScanCacheMinMs) && !s_scanInProgress) {
        refreshScanCache(true);
    } else if (clients > 0) {
        W20_WIFI_LOG("[WiFiProv] scanNetworks serve-cache only (clients=%d len=%u)\n", clients,
                      (unsigned)s_scanJsonCache.length());
    }

    s_http.sendHeader("Access-Control-Allow-Origin", "*");
    s_http.sendHeader("Cache-Control", "no-cache");
    s_http.send(200, "application/json", s_scanJsonCache);
    W20_WIFI_LOG("[WiFiProv] scanNetworks resp scanning=%d len=%u\n", s_scanInProgress ? 1 : 0,
                  (unsigned)s_scanJsonCache.length());
}

static void handleWifiStatus()
{
    if (s_staConnectOk || staHasValidIp()) {
        if (s_connectedStatusServed < 255) {
            s_connectedStatusServed++;
        }
        char ipBuf[16] = "0.0.0.0";
        if (s_staConnectIp[0] != '\0') {
            strncpy(ipBuf, s_staConnectIp, sizeof(ipBuf) - 1);
            ipBuf[sizeof(ipBuf) - 1] = '\0';
        } else if (staHasValidIp()) {
            snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
        }
        W20_WIFI_LOG("[WiFiProv] HTTP GET /json/wifi/status -> connected ip=%s served=%u\n", ipBuf,
                      (unsigned)s_connectedStatusServed);
        s_http.sendHeader("Cache-Control", "no-cache");
        s_http.send(200, "application/json",
                    String("{\"status\":\"ok\",\"connected\":true,\"failed\":false,\"ip\":\"") + ipBuf +
                        "\",\"message\":\"Connected\"}");
        return;
    }
    if (s_connectFailed) {
        s_http.send(200, "application/json",
                    String("{\"status\":\"ok\",\"connected\":false,\"failed\":true,\"message\":\"") + PROV_CONNECT_FAIL_MSG +
                        "\"}");
        return;
    }
    if (s_state == UI_WIFI_PROV_FAILED) {
        s_http.send(200, "application/json", "{\"status\":\"ok\",\"connected\":false,\"failed\":true,\"message\":\"AP start failed\"}");
        return;
    }
    if (s_state == UI_WIFI_PROV_CONNECTING) {
        s_http.send(200, "application/json",
                    "{\"status\":\"ok\",\"connected\":false,\"failed\":false,\"message\":\"Connecting...\"}");
        return;
    }
    s_http.send(200, "application/json", "{\"status\":\"ok\",\"connected\":false,\"failed\":false,\"message\":\"Idle\"}");
}

static void handleWifiConnect()
{
    W20_WIFI_LOGLN("[WiFiProv] HTTP POST /json/wifi/connect");
    if (s_http.method() != HTTP_POST) {
        s_http.send(405, "application/json", "{\"status\":\"error\",\"message\":\"POST only\"}");
        return;
    }
    String body = s_http.arg("plain");
    if (body.length() == 0 && s_http.args() > 0) {
        for (int i = 0; i < s_http.args(); i++) {
            if (s_http.argName(i) == "plain") {
                body = s_http.arg(i);
                break;
            }
        }
    }
    char ssid[33] = {0};
    char pass[65] = {0};
    if (!extractJsonString(body.c_str(), "ssid", ssid, sizeof(ssid))) {
        s_http.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing ssid\"}");
        return;
    }
    (void)extractJsonString(body.c_str(), "password", pass, sizeof(pass));

    String err;
    if (!queueStaConnectFromCredentials(ssid, pass, &err)) {
        s_http.send(400, "application/json",
                    String("{\"status\":\"error\",\"message\":\"") + err + "\"}");
        return;
    }
    s_http.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
}

/** 无 JS 表单：POST /connect （ssid / ssid_sel / password） */
static void handleFormConnect()
{
    W20_WIFI_LOGLN("[WiFiProv] HTTP POST /connect (form)");
    String ssid = s_http.hasArg("ssid") ? s_http.arg("ssid") : "";
    ssid.trim();
    if (ssid.length() == 0 && s_http.hasArg("ssid_sel")) {
        ssid = s_http.arg("ssid_sel");
        ssid.trim();
    }
    String pass = s_http.hasArg("password") ? s_http.arg("password") : "";
    String err;
    if (!queueStaConnectFromCredentials(ssid.c_str(), pass.c_str(), &err)) {
        String html = F("<!DOCTYPE html><html><body style=\"font-family:sans-serif;background:#111;color:#eee;padding:16px\">"
                        "<h3>Setup failed</h3><p>");
        html += err;
        html += F("</p><p><a href=\"/\" style=\"color:#3dd68c\">Back</a></p></body></html>");
        s_http.send(400, "text/html", html);
        return;
    }
    s_http.send(200, "text/html",
                F("<!DOCTYPE html><html><body style=\"font-family:sans-serif;background:#111;color:#eee;padding:16px\">"
                  "<h3>Connecting...</h3><p>Watch the device screen. You can close this page.</p>"
                  "<p><a href=\"/\" style=\"color:#3dd68c\">Back</a></p></body></html>"));
}

static void registerHttpRoutes()
{
    if (s_routesRegistered)
        return;
    s_routesRegistered = true;

    s_http.on("/", HTTP_GET, handleRoot);
    s_http.on("/connect", HTTP_POST, handleFormConnect);
    s_http.on("/json/scanNetworks", HTTP_GET, handleScanNetworks);
    s_http.on("/json/wifi/status", HTTP_GET, handleWifiStatus);
    s_http.on("/json/wifi/connect", HTTP_POST, handleWifiConnect);

    s_http.on("/generate_204", HTTP_GET, sendPortalRedirect);
    s_http.on("/gen_204", HTTP_GET, sendPortalRedirect);
    s_http.on("/hotspot-detect.html", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/library/test/success.html", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/connecttest.txt", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/ncsi.txt", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/redirect", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/canonical.html", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/success.txt", HTTP_GET, sendCaptivePortalPage);
    s_http.on("/fwlink", HTTP_GET, sendCaptivePortalPage);

    s_http.onNotFound([]() {
        W20_WIFI_LOG("[WiFiProv] HTTP %s %s\n", s_http.method() == HTTP_GET ? "GET" : "OTHER", s_http.uri().c_str());
        if (s_http.method() == HTTP_GET) {
            if (s_http.uri().startsWith("/generate_204") || s_http.uri().startsWith("/gen_204")) {
                sendPortalRedirect();
            } else {
                sendCaptivePortalPage();
            }
        } else {
            s_http.send(404, "text/plain", "Not found");
        }
    });
}

/** Deauth AP clients before softAPdisconnect — avoids ESP WiFi stack panic when a PC stays associated. */
static void deauthSoftApClients(void)
{
    const int n = WiFi.softAPgetStationNum();
    if (n <= 0) {
        return;
    }
    W20_WIFI_LOG("[WiFiProv] deauth %d AP client(s) before AP teardown\n", n);
    esp_wifi_deauth_sta(0); /* aid 0 = all associated stations */
}

void ui_wifi_force_ap_teardown(bool keep_sta)
{
    if (s_running || s_dnsRunning) {
        s_http.stop();
        delay(50);
        s_dns.stop();
        s_dnsRunning = false;
    }

    if (s_softapUp || WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        deauthSoftApClients();
        if (WiFi.softAPgetStationNum() > 0) {
            delay(300);
        }
        /* false：只关 SoftAP，勿 wifioff=true（会把 STA 一并掐掉） */
        WiFi.softAPdisconnect(false);
        s_softapUp = false;
        s_softapBoostPending = false;
    }

    const wifi_mode_t mode = WiFi.getMode();

    /* 用户主动断连重配：拆 SoftAP 后必须 WIFI_OFF，勿留空 STA。
     * 留 WIFI_STA 再起 SoftAP 时 HTTP 会卡约 10s（日志：GET / 到 html len 差 10s），
     * 退出整个 WiFi 菜单（STA off）后再进则正常。 */
    if (s_userHoldOffline) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        w20_wifi_abort_sta_reconnect();
#endif
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(true, true);
        delay(50);
        WiFi.mode(WIFI_OFF);
        delay(150);
        W20_WIFI_LOGLN("[WiFiProv] SoftAP down, hold offline (WIFI_OFF, no STA restore)");
        return;
    }

    if (keep_sta) {
        if (mode != WIFI_STA) {
            WiFi.mode(WIFI_STA);
            esp_wifi_set_mode(WIFI_MODE_STA);
            delay(80);
        }
        /* SoftAP 拆除后偶发丢关联：用已存凭据拉回 STA */
        if (WiFi.status() != WL_CONNECTED && config.network.wifi_ssid[0]) {
            const char *psk = config.network.wifi_psk[0] ? config.network.wifi_psk : nullptr;
            W20_WIFI_LOGLN("[WiFiProv] STA lost after SoftAP teardown, re-begin");
            WiFi.begin(config.network.wifi_ssid, psk);
        }
    } else if (mode == WIFI_AP || mode == WIFI_AP_STA || mode == WIFI_OFF) {
        /* 未连上也不要 WIFI_OFF：有保存 SSID 则切回 STA 并重连，否则 OTA/Scan 会 Need WiFi */
        WiFi.mode(WIFI_STA);
        esp_wifi_set_mode(WIFI_MODE_STA);
        delay(100);
        if (WiFi.status() != WL_CONNECTED && config.network.wifi_ssid[0]) {
            const char *psk = config.network.wifi_psk[0] ? config.network.wifi_psk : nullptr;
            W20_WIFI_LOG("[WiFiProv] SoftAP down → restore STA ssid=%s\n", config.network.wifi_ssid);
            WiFi.disconnect(false, false);
            delay(50);
            WiFi.begin(config.network.wifi_ssid, psk);
            WiFi.setAutoReconnect(true);
        }
    }
}

/** Stop portal HTTP/DNS then tear down SoftAP. Caller must clear s_running first so provTask skips servicePortal(). */
static void teardownPortalServices()
{
    ui_wifi_force_ap_teardown(true);
}

void ui_wifi_pause_ble_for_radio(void)
{
#if defined(ARCH_ESP32) && !MESHTASTIC_EXCLUDE_BLUETOOTH && HAS_BLUETOOTH
    if (nimbleBluetooth && nimbleBluetooth->isActive()) {
        W20_WIFI_LOGLN("[WiFiProv] Pause Meshtastic BLE for WiFi radio");
        nimbleBluetooth->deinit();
        delay(200);
    }
#endif
#if defined(BLE_PIN_CODE)
    /* MeshCore companion Arduino BLE — stop advertising; PS_MIN_MODEM still required. */
    extern class SerialBLEInterface bluetooth_interface;
    if (bluetooth_interface.isEnabled()) {
        W20_WIFI_LOGLN("[WiFiProv] Pause MeshCore BLE for WiFi radio");
        bluetooth_interface.disable();
        delay(50);
    }
#endif
}

static void pauseBluetoothForWifi(void)
{
    ui_wifi_pause_ble_for_radio();
}

static void meshApSsidFromMac(char *buf, size_t len)
{
    uint8_t dmac[6];
    getMacAddr(dmac);
    snprintf(buf, len, "Mesh-%02x%02x%02x", dmac[3], dmac[4], dmac[5]);
}

/** Boot has "Not using WIFI" when wifi_enabled=0 — bring up radio cleanly for AP.
 * 配网 SoftAP 用纯 WIFI_AP：AP_STA 下空闲 STA 易导致手机连上后门户页半截/进不去。
 * 提交账号连路由器时再在 applyPendingConnect() 切回 AP_STA。 */
static void prepareWifiRadioForAp()
{
    pauseBluetoothForWifi();

#if !MESHTASTIC_EXCLUDE_WEBSERVER
    stopWebServer();
#endif

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_wifi_abort_sta_reconnect();
#endif

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    /* 彻底拆 STA；停在 WIFI_OFF，由 startSoftApPortal 一次性 softAP。
     * 上次 SoftAP→STA 若只 softAPdisconnect 而未 WIFI_OFF，二次 SoftAP 的 HTTP 会卡 ~10s。 */
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(600);
    W20_WIFI_LOGLN("[WiFiProv] radio: WIFI_OFF ready for SoftAP portal");
}

/** OTA SoftAP 专用：纯 AP 模式，避免 AP_STA 下 STA 侧扫描/重连导致 PC 掉线。 */
static void prepareWifiRadioForOtaAp()
{
    pauseBluetoothForWifi();

#if !MESHTASTIC_EXCLUDE_WEBSERVER
    stopWebServer();
#endif

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(250);
    WiFi.mode(WIFI_AP);
    delay(150);
    w20_wifi_apply_ps_coex_safe();
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    W20_WIFI_LOGLN("[WiFiProv] radio: pure WIFI_AP for OTA");
}

/** Reconfig while STA linked: add SoftAP without dropping router connection. */
static void prepareWifiRadioForApKeepSta()
{
    pauseBluetoothForWifi();

#if !MESHTASTIC_EXCLUDE_WEBSERVER
    stopWebServer();
#endif

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        delay(80);
    }
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
        delay(150);
    }
    w20_wifi_apply_ps_coex_safe();
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
}

/** 与 initWifi() 一致的 STA 侧设置（hostname / 静态 IP / 省电关闭）。 */
static void configureStaLikeMeshtasticBoot(void)
{
    uint8_t dmac[6];
    getMacAddr(dmac);
    char host[16];
    snprintf(host, sizeof(host), "Meshtastic-%02x%02x", dmac[4], dmac[5]);
    WiFi.setHostname(host);

    if (config.network.address_mode == meshtastic_Config_NetworkConfig_AddressMode_STATIC &&
        config.network.ipv4_config.ip != 0) {
        WiFi.config(config.network.ipv4_config.ip, config.network.ipv4_config.gateway, config.network.ipv4_config.subnet,
                    config.network.ipv4_config.dns);
    }
    w20_wifi_apply_ps_coex_safe();
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
}

static int s_softapChannel = 6;

/** 选 SoftAP 信道 / 扫描时缓存 SSID→信道，连路由器时复用，避免有手机连着 SoftAP 再扫网踢人。 */
static constexpr int kSsidChCacheMax = 24;
static char s_ssidChName[kSsidChCacheMax][33];
static int8_t s_ssidChVal[kSsidChCacheMax];
static int8_t s_ssidChRssi[kSsidChCacheMax];
static int s_ssidChCount = 0;

static void rememberSsidChannelsFromScan(int apCount)
{
    s_ssidChCount = 0;
    if (apCount <= 0) {
        return;
    }
    for (int i = 0; i < apCount && s_ssidChCount < kSsidChCacheMax; i++) {
        String ssidStr = WiFi.SSID(i);
        const char *ssid = ssidStr.c_str();
        if (!ssid || ssid[0] == '\0') {
            continue;
        }
        const int ch = WiFi.channel(i);
        if (ch < 1 || ch > 13) {
            continue;
        }
        const int rssi = WiFi.RSSI(i);
        int found = -1;
        for (int j = 0; j < s_ssidChCount; j++) {
            if (strcmp(s_ssidChName[j], ssid) == 0) {
                found = j;
                break;
            }
        }
        if (found >= 0) {
            if (rssi > s_ssidChRssi[found]) {
                s_ssidChVal[found] = (int8_t)ch;
                s_ssidChRssi[found] = (int8_t)rssi;
            }
            continue;
        }
        strncpy(s_ssidChName[s_ssidChCount], ssid, sizeof(s_ssidChName[0]) - 1);
        s_ssidChName[s_ssidChCount][sizeof(s_ssidChName[0]) - 1] = '\0';
        s_ssidChVal[s_ssidChCount] = (int8_t)ch;
        s_ssidChRssi[s_ssidChCount] = (int8_t)rssi;
        s_ssidChCount++;
    }
}

static int lookupCachedSsidChannel(const char *ssid)
{
    if (!ssid || ssid[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < s_ssidChCount; i++) {
        if (strcmp(s_ssidChName[i], ssid) == 0) {
            W20_WIFI_LOG("[WiFiProv] cached channel for %s -> ch=%d\n", ssid, (int)s_ssidChVal[i]);
            return s_ssidChVal[i];
        }
    }
    return -1;
}

/** 用选信道扫描留下的 SSID 表重建 JSON（不触发射频 scan）。 */
static void rebuildScanJsonFromSsidCache(void)
{
    if (s_ssidChCount <= 0) {
        return;
    }
    /* 始终按当前信道扫描缓存重建，避免二次 SoftAP 仍用旧 JSON */
    constexpr int kMaxJson = 8;
    int idx[kSsidChCacheMax];
    int nAll = 0;
    for (int i = 0; i < s_ssidChCount && nAll < kSsidChCacheMax; i++) {
        if (s_ssidChName[i][0] == '\0')
            continue;
        idx[nAll++] = i;
    }
    for (int i = 0; i < nAll; i++) {
        for (int j = i + 1; j < nAll; j++) {
            if (s_ssidChRssi[idx[j]] > s_ssidChRssi[idx[i]]) {
                const int t = idx[i];
                idx[i] = idx[j];
                idx[j] = t;
            }
        }
    }
    const int n = (nAll > kMaxJson) ? kMaxJson : nAll;

    String data = "[";
    char esc[64];
    for (int k = 0; k < n; k++) {
        const int i = idx[k];
        if (data.length() > 1)
            data += ',';
        jsonEscape(s_ssidChName[i], esc, sizeof(esc));
        data += "{\"ssid\":\"";
        data += esc;
        data += "\",\"rssi\":";
        data += String((int)s_ssidChRssi[i]);
        data += '}';
    }
    data += "]";
    String json = "{\"status\":\"ok\",\"scanning\":false,\"data\":";
    json += data;
    json += "}";
    s_scanJsonCache = json;
    if (s_lastScanMs == 0) {
        s_lastScanMs = millis();
    }
    W20_WIFI_LOG("[WiFiProv] rebuilt scan JSON from ssid cache n=%d (of %d) len=%u\n", n, nAll,
                  (unsigned)s_scanJsonCache.length());
}

static bool queueStaConnectFromCredentials(const char *ssid, const char *pass, String *errOut)
{
    if (!ssid || ssid[0] == '\0') {
        if (errOut)
            *errOut = "Missing ssid";
        return false;
    }
    if (!pass || pass[0] == '\0') {
        if (errOut)
            *errOut = "Password required";
        return false;
    }
    if (strlen(pass) < 8) {
        Serial.printf("[WiFiProv] connect rejected: password len=%u < 8\n", (unsigned)strlen(pass));
        s_connectFailed = true;
        s_connectPending = false;
        if (s_state == UI_WIFI_PROV_CONNECTING)
            s_state = UI_WIFI_PROV_AP;
        provNotifyUiIfChanged();
        if (errOut)
            *errOut = PROV_CONNECT_FAIL_MSG;
        return false;
    }

    strncpy(s_pendingSsid, ssid, sizeof(s_pendingSsid) - 1);
    s_pendingSsid[sizeof(s_pendingSsid) - 1] = '\0';
    strncpy(s_pendingPass, pass, sizeof(s_pendingPass) - 1);
    s_pendingPass[sizeof(s_pendingPass) - 1] = '\0';
    W20_WIFI_LOG("[WiFiProv] queue connect ssid=%s pass_len=%u\n", s_pendingSsid,
                  (unsigned)strlen(s_pendingPass));
    s_connectFailed = false;
    s_connectPending = true;
    s_state = UI_WIFI_PROV_CONNECTING;
    s_connectStartMs = millis();
    s_connectLastLogMs = 0;
    s_lastLoggedDiscReason = 255;
    resetConnectProgressFlags();
    clearWifiDisconnectReason();
    provLogStaSnapshot("connect_accepted");
    return true;
}

/** 2.4GHz 邻道干扰加权：分数越低越适合开 SoftAP。 */
static int softApChannelCongestion(int targetCh, int apCount)
{
    int score = 0;
    for (int i = 0; i < apCount; i++) {
        const int ch = WiFi.channel(i);
        const int diff = (ch > targetCh) ? (ch - targetCh) : (targetCh - ch);
        if (diff <= 4) {
            score += (5 - diff);
        }
    }
    return score;
}

/** 首次开 AP 前扫描，在 1/6/11 中选干扰最小的信道。 */
static int pickSoftApChannel(void)
{
    static const int candidates[] = {1, 6, 11};
    int bestCh = 6;
    int bestScore = INT_MAX;

    W20_WIFI_LOGLN("[WiFiProv] scan for SoftAP channel (1/6/11)...");
    const int n = WiFi.scanNetworks(false, true);
    if (n == WIFI_SCAN_FAILED || n < 0) {
        WiFi.scanDelete();
        Serial.println("[WiFiProv] scan failed, default ch=6");
        return 6;
    }

    for (int c = 0; c < 3; c++) {
        const int ch = candidates[c];
        const int score = softApChannelCongestion(ch, n);
        W20_WIFI_LOG("[WiFiProv] ch=%d congestion=%d\n", ch, score);
        if (score < bestScore) {
            bestScore = score;
            bestCh = ch;
        }
    }
    /* 复用信道扫描结果作门户首屏 SSID 列表，避免手机连上后再 scan 掐断 SoftAP */
    rememberSsidChannelsFromScan(n);
    setScanJsonCache(n, false);
    s_lastScanMs = millis();
    WiFi.scanDelete();
    W20_WIFI_LOG("[WiFiProv] pick SoftAP channel %d (score=%d, %d APs seen)\n", bestCh, bestScore, n);
    return bestCh;
}

static void applySoftApRadioBoost(void)
{
    /* 只关省电；勿 set_bandwidth / setTxPower（ESP32 上会再 stop/start SoftAP）。 */
    w20_wifi_apply_ps_coex_safe();
}

/** softAP 成功后不再延迟 boost（曾导致 AP stopped/started，手机门户进不去）。 */
static void scheduleSoftApRadioBoost(unsigned long delayMs)
{
    (void)delayMs;
    s_softapBoostPending = false;
    s_softapBoostDueMs = 0;
}

static void maybeApplyDeferredSoftApRadioBoost(void)
{
    s_softapBoostPending = false;
}

/**
 * 同步扫描目标 SSID，取最强 AP 的信道（1–13）。
 * AP_STA 下 scan 会打断 SoftAP，有手机连着时禁止调用。
 */
static int findTargetSsidChannel(const char *ssid)
{
    if (!ssid || ssid[0] == '\0')
        return -1;

    if (WiFi.softAPgetStationNum() > 0) {
        W20_WIFI_LOGLN("[WiFiProv] skip live target scan (SoftAP has client)");
        return -1;
    }

    if (s_scanInProgress) {
        WiFi.scanDelete();
        s_scanInProgress = false;
        W20_WIFI_LOGLN("[WiFiProv] abort async scan before target scan");
    }

    W20_WIFI_LOG("[WiFiProv] scan target ssid=%s for SoftAP align...\n", ssid);
    const int n = WiFi.scanNetworks(false, true);
    if (n == WIFI_SCAN_FAILED || n < 0) {
        WiFi.scanDelete();
        Serial.println("[WiFiProv] target scan failed");
        return -1;
    }

    rememberSsidChannelsFromScan(n);

    int bestCh = -1;
    int bestRssi = -127;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) != ssid)
            continue;
        const int rssi = WiFi.RSSI(i);
        const int ch = WiFi.channel(i);
        if (ch < 1 || ch > 13)
            continue;
        if (bestCh < 0 || rssi > bestRssi) {
            bestCh = ch;
            bestRssi = rssi;
        }
    }
    WiFi.scanDelete();

    if (bestCh > 0) {
        W20_WIFI_LOG("[WiFiProv] target ssid found ch=%d rssi=%d\n", bestCh, bestRssi);
    } else {
        W20_WIFI_LOGLN("[WiFiProv] target ssid not in scan (keep SoftAP ch)");
    }
    return bestCh;
}

/** 将 SoftAP 切到与目标路由器同信道，减轻 AP+STA 单射频冲突。 */
static bool realignSoftApToChannel(int ch)
{
    if (ch < 1 || ch > 13 || s_apSsid[0] == '\0')
        return false;
    if (s_softapUp && s_softapChannel == ch) {
        W20_WIFI_LOG("[WiFiProv] SoftAP already on ch=%d\n", ch);
        return true;
    }

    const IPAddress apIp(192, 168, 4, 1);
    WiFi.mode(WIFI_AP_STA);
    delay(50);
    if (!WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0))) {
        Serial.println("[WiFiProv] softAPConfig failed (realign)");
    }
    if (s_softapUp) {
        WiFi.softAPdisconnect(false);
        delay(100);
        s_softapUp = false;
        s_softapBoostPending = false;
    }
    if (!WiFi.softAP(s_apSsid, "", ch, 0, 4)) {
        Serial.printf("[WiFiProv] SoftAP realign failed ch=%d\n", ch);
        return false;
    }
    s_softapChannel = ch;
    s_softapUp = true;
    w20_wifi_apply_ps_coex_safe();
    scheduleSoftApRadioBoost(0);
    if (s_dnsRunning) {
        s_dns.stop();
        s_dns.start(53, "*", apIp);
    }
    W20_WIFI_LOG("[WiFiProv] SoftAP realigned ssid=%s ch=%d\n", s_apSsid, ch);
    return true;
}

static void alignSoftApToPendingSsid(void)
{
    const int clients = WiFi.softAPgetStationNum();
    int ch = lookupCachedSsidChannel(s_pendingSsid);
    if (ch < 0 && clients == 0) {
        ch = findTargetSsidChannel(s_pendingSsid);
    } else if (ch < 0) {
        W20_WIFI_LOGLN("[WiFiProv] no cached channel; keep SoftAP ch (phone online)");
        ch = s_softapChannel > 0 ? s_softapChannel : 6;
    }

    s_deferredAlignChannel = -1;
    if (ch > 0 && ch != s_softapChannel) {
        if (clients > 0) {
            /* 改 SoftAP 信道会踢手机；记下目标信道，等客户端离开或 AUTH 重试时再对齐 */
            s_deferredAlignChannel = ch;
            W20_WIFI_LOG("[WiFiProv] skip SoftAP realign (clients=%d) keep ch=%d target=%d (deferred)\n", clients,
                          s_softapChannel, ch);
        } else if (!realignSoftApToChannel(ch)) {
            (void)realignSoftApToChannel(s_softapChannel > 0 ? s_softapChannel : 6);
        }
    } else if (!s_softapUp) {
        (void)realignSoftApToChannel(s_softapChannel > 0 ? s_softapChannel : 6);
    }
}

/** 若有暂缓对齐且 SoftAP 上已无手机（或 force），把 SoftAP 切到目标信道。 */
static bool applyDeferredSoftApAlign(bool force, const char *tag)
{
    if (s_deferredAlignChannel <= 0 || s_deferredAlignChannel == s_softapChannel) {
        s_deferredAlignChannel = -1;
        return false;
    }
    const int clients = WiFi.softAPgetStationNum();
    if (clients > 0 && !force) {
        return false;
    }
    const int target = s_deferredAlignChannel;
    W20_WIFI_LOG("[WiFiProv] SoftAP realign %s ch=%d -> %d (clients=%d force=%d)\n", tag, s_softapChannel, target,
                  clients, force ? 1 : 0);
    if (!realignSoftApToChannel(target)) {
        Serial.printf("[WiFiProv] SoftAP realign failed (%s)\n", tag);
        return false;
    }
    s_deferredAlignChannel = -1;
    return true;
}

static void staBeginNow(const char *tag)
{
    const char *pass = s_pendingPass[0] ? s_pendingPass : nullptr;
    W20_WIFI_LOG("[WiFiProv] STA begin (%s) ssid=%s pass_len=%u softap_ch=%d\n", tag, s_pendingSsid,
                  (unsigned)strlen(s_pendingPass), s_softapChannel);
    clearWifiDisconnectReason();
    WiFi.disconnect(false, false);
    delay(80);
    WiFi.begin(s_pendingSsid, pass);
    needReconnect = false;
    s_lastStaBeginMs = millis();
    s_lastLoggedDiscReason = 255;
    provLogStaSnapshot(tag);
}

static bool startSoftApPortal(const IPAddress &apIp, const char *ssid, bool scan_for_channel)
{
    if (!ssid || ssid[0] == '\0') {
        return false;
    }

    if (scan_for_channel) {
        /* 仅当调用方未预选信道时才扫描；扫描会短暂关掉 SoftAP，门户期应尽量避免 */
        const wifi_mode_t prev = WiFi.getMode();
        if (prev == WIFI_AP || prev == WIFI_AP_STA) {
            WiFi.mode(WIFI_STA);
            delay(80);
        } else if (prev == WIFI_OFF) {
            WiFi.mode(WIFI_STA);
            delay(80);
        }
        s_softapChannel = pickSoftApChannel();
        WiFi.disconnect(true, true);
        delay(50);
        WiFi.mode(WIFI_OFF);
        delay(80);
    }

    /* 从 OFF/干净态一次 softAP：勿先 WiFi.mode(WIFI_AP) 空启，否则 softAP 会再 stop/start。 */
    {
        const wifi_mode_t m = WiFi.getMode();
        if (m == WIFI_AP_STA || m == WIFI_STA || m == WIFI_AP) {
            WiFi.disconnect(true, true);
            delay(50);
            WiFi.mode(WIFI_OFF);
            delay(100);
        } else if (m != WIFI_OFF) {
            WiFi.mode(WIFI_OFF);
            delay(100);
        }
    }

    if (s_softapChannel < 1 || s_softapChannel > 13) {
        s_softapChannel = 6;
    }

    /* softAP 前配置网关/DNS=AP IP，否则手机 DHCP 无 DNS，captive 探测走公网 → 无弹窗/无网络 */
    WiFi.mode(WIFI_AP);
    delay(120);
    if (!WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0))) {
        W20_WIFI_LOGLN("[WiFiProv] softAPConfig failed (pre-softAP)");
    }
    delay(40);

    int tryOrder[3];
    tryOrder[0] = s_softapChannel;
    int nTry = 1;
    static const int fallbacks[] = {1, 6, 11};
    for (int fb : fallbacks) {
        if (fb != s_softapChannel) {
            tryOrder[nTry++] = fb;
        }
    }

    for (int i = 0; i < nTry; i++) {
        const int ch = tryOrder[i];
        /* softAPConfig 已在上方完成；此处只起 AP，避免 softAP 后再 config 触发 stop/start。 */
        if (WiFi.softAP(ssid, "", ch, 0, 4)) {
            s_softapChannel = ch;
            /* 仅关省电；勿立刻/延迟 set_bandwidth / setTxPower（会再 stop/start AP）。 */
            w20_wifi_apply_ps_coex_safe();
                    /* softAP() 内部常再抖一次 started/stopped；等稳再开 HTTP，减少门户首包卡死 */
            delay(350);
            W20_WIFI_LOG("[WiFiProv] softAP OK ssid=%s ch=%d ip=%s\n", ssid, ch, WiFi.softAPIP().toString().c_str());
            s_softapUp = true;
            scheduleSoftApRadioBoost(0);
            return true;
        }
        W20_WIFI_LOG("[WiFiProv] softAP retry ch=%d mode=%d\n", ch, (int)WiFi.getMode());
        WiFi.softAPdisconnect(false);
        delay(100);
        WiFi.mode(WIFI_AP);
        delay(80);
        (void)WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
        delay(40);
    }

    Serial.printf("[WiFiProv] softAP failed mode=%d\n", (int)WiFi.getMode());
    return false;
}

bool ui_wifi_softap_begin(bool keep_sta, bool ota_ap_only)
{
    if (s_softapUp) {
        return true;
    }
    meshApSsidFromMac(s_apSsid, sizeof(s_apSsid));
    if (ota_ap_only) {
        prepareWifiRadioForOtaAp();
    } else if (keep_sta) {
        prepareWifiRadioForApKeepSta();
    } else {
        prepareWifiRadioForAp();
    }
    const IPAddress apIp(192, 168, 4, 1);
    return startSoftApPortal(apIp, s_apSsid, true);
}

void ui_wifi_softap_end(bool keep_sta)
{
    if (!s_softapUp && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        return;
    }
    ui_wifi_force_ap_teardown(keep_sta);
}

bool ui_wifi_softap_is_up(void)
{
    return s_softapUp;
}

void ui_wifi_softap_get_ssid(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }
    strncpy(buf, s_apSsid, len - 1);
    buf[len - 1] = '\0';
}

static bool isFatalWifiDisconnectReason(uint8_t reason)
{
    switch (reason) {
    case 15:  /* WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT */
    case 201: /* WIFI_REASON_NO_AP_FOUND */
    case 202: /* WIFI_REASON_AUTH_FAIL */
    case 204: /* WIFI_REASON_HANDSHAKE_TIMEOUT */
    case 205: /* WIFI_REASON_CONNECTION_FAIL */
        return true;
    default:
        return false;
    }
}

/** SoftAP/STA 跨信道时常见、值得重试（含对齐信道后再 begin）。 */
static bool isChannelSensitiveDisconnectReason(uint8_t reason)
{
    switch (reason) {
    case 2:  /* AUTH_EXPIRE */
    case 15: /* 4WAY_HANDSHAKE_TIMEOUT */
    case 204: /* HANDSHAKE_TIMEOUT */
    case 205: /* CONNECTION_FAIL */
        return true;
    default:
        return false;
    }
}

static void saveWifiCredentialsFromPending(void)
{
    strncpy(config.network.wifi_ssid, s_pendingSsid, sizeof(config.network.wifi_ssid) - 1);
    config.network.wifi_ssid[sizeof(config.network.wifi_ssid) - 1] = '\0';
    strncpy(config.network.wifi_psk, s_pendingPass, sizeof(config.network.wifi_psk) - 1);
    config.network.wifi_psk[sizeof(config.network.wifi_psk) - 1] = '\0';
    config.network.wifi_enabled = true;
    if (nodeDB) {
        nodeDB->saveToDisk(SEGMENT_CONFIG);
    }
}

static void ensurePortalForUserRetry(void)
{
    const IPAddress apIp(192, 168, 4, 1);
    if (!s_softapUp) {
        if (!startSoftApPortal(apIp, s_apSsid, false)) {
            s_state = UI_WIFI_PROV_FAILED;
            Serial.println("[WiFiProv] portal restore: softAP failed");
            return;
        }
    }
    if (!s_dnsRunning) {
        s_dns.start(53, "*", apIp);
        s_dnsRunning = true;
    }
    if (!s_routesRegistered) {
        registerHttpRoutes();
    }
    s_http.begin();
    s_running = true;
    s_state = UI_WIFI_PROV_AP;
    s_apWatchLastMs = millis();
}

static void markConnectFailed(const char *logTag)
{
    if (s_stopRequested || s_connectFailed) {
        return;
    }
    s_connectFailed = true;
    resetConnectProgressFlags();
    s_connectPending = false;
    Serial.printf("[WiFiProv] Connect failed (%s) disc_reason=%u(%s)\n", logTag, (unsigned)getWifiDisconnectReason(),
                  wifiReasonStr(getWifiDisconnectReason()));
    provLogStaSnapshot("connect_failed");
    WiFi.disconnect(false, false);
    ensurePortalForUserRetry();
    provNotifyUiIfChanged();
}

static void applyPendingConnect()
{
    if (!s_connectPending || s_stopRequested)
        return;
    s_connectPending = false;

    resolvePendingCredentials();

    provLogStaSnapshot("before_sta_connect");

    pauseBluetoothForWifi();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    configureStaLikeMeshtasticBoot();

    /* SoftAP 与路由器同信道，再 begin；否则单射频下 STA 易卡在 DISCONNECTED/reason=0 */
    alignSoftApToPendingSsid();

    s_staBeginRetryCount = 0;
    s_authRetryCount = 0;
    s_lastAuthRetryMs = 0;
    staBeginNow("sta_begin");
}

static void servicePortal()
{
    const int clients = WiFi.softAPgetStationNum();
    const bool connecting = (s_state == UI_WIFI_PROV_CONNECTING);
    /* 门户期抬高 DNS/HTTP 频次：手机刚连上 clients 可能短时为 0，过低会丢 captive 探测 */
    const int dnsPasses = (clients > 0 || connecting || s_running) ? 24 : 4;
    const int httpPasses = (clients > 0 || connecting || s_running) ? 8 : 1;
    for (int i = 0; i < dnsPasses; i++) {
        s_dns.processNextRequest();
    }
    for (int i = 0; i < httpPasses; i++) {
        s_http.handleClient();
    }
}

static bool beginPortalServices(const IPAddress &apIp, bool scan_for_channel)
{
    if (!startSoftApPortal(apIp, s_apSsid, scan_for_channel)) {
        s_state = UI_WIFI_PROV_FAILED;
        return false;
    }

    /* 每次起门户：先停再开 HTTP，避免上次 SoftAP 会话残留导致首包 TCP 卡死 */
    s_http.stop();
    delay(30);
    s_dns.stop();
    s_dns.start(53, "*", apIp);
    s_dnsRunning = true;
    if (!s_routesRegistered) {
        registerHttpRoutes();
    }
    s_http.begin();

    s_running = true;
    s_state = UI_WIFI_PROV_AP;
    s_connectPending = false;
    /* 保留选信道时写入的扫描缓存时间戳；清零会让 HTTP scan 在 clients==0 时同步扫网并打掉 SoftAP */
    if (s_lastScanMs == 0) {
        s_lastScanMs = millis();
    }
    s_apWatchLastMs = millis();
    W20_WIFI_LOG("[WiFiProv] AP ready %s http://%s (scan deferred until phone HTTP)\n", s_apSsid, apIp.toString().c_str());
    return true;
}

/** Run only from provTask — avoids tearing down HTTP while handleClient() is active. */
static void performStopInProvTask(void)
{
    /* 若 stop 期间又点进 Config，保留重启意图（勿被下面清掉）。 */
    const bool restartAfter = s_pendingStartAfterStop || s_startRequested;
    s_stopRequested = false;
    s_startRequested = false;
    s_pendingStartAfterStop = false;
    s_connectFailed = false;
    s_connectPending = false;
    resetConnectProgressFlags();

    const bool keepSta =
        !restartAfter && !s_userHoldOffline && (s_state == UI_WIFI_PROV_CONNECTED || WiFi.status() == WL_CONNECTED);

    if (s_state == UI_WIFI_PROV_CONNECTING && WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false, false);
        delay(80);
    }

    s_running = false;
    if (s_softapUp || s_dnsRunning || s_state == UI_WIFI_PROV_CONNECTING || s_state == UI_WIFI_PROV_AP ||
        s_state == UI_WIFI_PROV_STARTING || WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        ui_wifi_force_ap_teardown(keepSta);
    }

    if (keepSta) {
        needReconnect = false;
    }

    s_state = UI_WIFI_PROV_IDLE;
    s_uiNotifyFp = UINT32_MAX;
    if (s_taskHandle != nullptr) {
        vTaskPrioritySet(s_taskHandle, 1);
    }
    W20_WIFI_LOGLN("[WiFiProv] Stopped (STA kept if connected)");

    if (restartAfter) {
        s_keepStaOnStart = false;
        meshApSsidFromMac(s_apSsid, sizeof(s_apSsid));
        s_startRequested = true;
        s_state = UI_WIFI_PROV_STARTING;
        W20_WIFI_LOGLN("[WiFiProv] restart SoftAP after stop");
        provNotifyUiIfChanged();
    }
}

static void provisioningWork()
{
    if (s_stopRequested) {
        performStopInProvTask();
        return;
    }
    if (s_startRequested) {
        s_startRequested = false;
        if (s_stopRequested) {
            return;
        }
        s_state = UI_WIFI_PROV_STARTING;
        provNotifyUiIfChanged();
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        w20_wifi_abort_sta_reconnect();
#endif

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        /* 等 UI 刷完 SoftAP 指引（~1.3s FULL）再动射频；超时也强制停墨 */
        {
            const uint32_t t0 = millis();
            while (!factory_epd_flush_suppressed() && (millis() - t0) < 1600u) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            factory_set_epd_flush_suppressed(true);
            W20_WIFI_LOGLN("[WiFiProv] EPD suppressed before SoftAP radio");
        }
#endif

        /*
         * 关键 SoftAP 前在 STA 模式下选信道，避免 startSoftApPortal(scan=true) 再走
         * WIFI_AP→STA→scan→AP 折叠：手机刚连上就掉、192.168.4.1 打不开。
         */
        bool needChannelScan = true;
        if (!s_keepStaOnStart) {
            const wifi_mode_t mode = WiFi.getMode();
            if (mode == WIFI_STA || mode == WIFI_AP_STA || mode == WIFI_AP) {
                if (mode == WIFI_AP) {
                    WiFi.mode(WIFI_STA);
                    delay(80);
                }
                s_softapChannel = pickSoftApChannel();
                needChannelScan = false;
            }
            prepareWifiRadioForAp();
        } else {
            prepareWifiRadioForApKeepSta();
        }
        const IPAddress apIp(192, 168, 4, 1);
        if (!beginPortalServices(apIp, needChannelScan)) {
            Serial.println("[WiFiProv] beginPortalServices failed");
        }
        /* SoftAP 门户期间抬高配网任务优先级，减轻墨水/UI 抢 CPU 导致 HTTP 卡死 */
        if (s_taskHandle != nullptr) {
            vTaskPrioritySet(s_taskHandle, 3);
        }
        provNotifyUiIfChanged();
    }

    if (s_running || s_state == UI_WIFI_PROV_CONNECTING) {
        servicePortal();
        maybeApplyDeferredSoftApRadioBoost();
        if (s_state == UI_WIFI_PROV_AP || s_state == UI_WIFI_PROV_CONNECTING) {
            finishAsyncScanIfReady();
        }
        /* 禁止 AP 就绪后自动 scan：会立刻关掉 SoftAP，手机扫不到 Mesh-xxxxxx */
        if (s_state == UI_WIFI_PROV_AP && !s_scanInProgress &&
            (s_apWatchLastMs == 0 || (millis() - s_apWatchLastMs) >= 5000)) {
            s_apWatchLastMs = millis();
            ensureSoftApPortalUp("watch");
        }
    }

    if (s_connectPending) {
        applyPendingConnect();
    }

    if (s_state == UI_WIFI_PROV_CONNECTING) {
        const unsigned long elapsed = millis() - s_connectStartMs;
        uint8_t discReason = getWifiDisconnectReason();
        if (discReason != s_lastLoggedDiscReason) {
            s_lastLoggedDiscReason = discReason;
            if (discReason != 0) {
                W20_WIFI_LOG("[WiFiProv] STA event disc_reason=%u(%s) elapsed=%lums\n", (unsigned)discReason,
                              wifiReasonStr(discReason), elapsed);
                provLogStaSnapshot("disc_reason_change");
            }
        }
        if (s_connectLastLogMs == 0 || (millis() - s_connectLastLogMs) >= 3000) {
            s_connectLastLogMs = millis();
            W20_WIFI_LOG("[WiFiProv] connecting elapsed=%lus\n", elapsed / 1000);
            provLogStaSnapshot("poll");
        }

        /* SoftAP 与路由器不同信道且手机已离开：立刻对齐再 begin */
        if (!staHasValidIp() && !s_stopRequested && applyDeferredSoftApAlign(false, "clients_gone")) {
            clearWifiDisconnectReason();
            s_lastLoggedDiscReason = 255;
            staBeginNow("sta_ch_align");
            discReason = getWifiDisconnectReason();
        }

        /* 仍 DISCONNECTED 且无 disc_reason：中途再 begin，缓解 AP+STA 一次 begin 卡住 */
        if (!staHasValidIp() && !s_stopRequested && WiFi.status() == WL_DISCONNECTED && discReason == 0 &&
            s_staBeginRetryCount < kStaBeginMaxExtraRetries &&
            s_lastStaBeginMs != 0 && (millis() - s_lastStaBeginMs) >= kStaBeginRetryIntervalMs) {
            s_staBeginRetryCount++;
            char tag[24];
            snprintf(tag, sizeof(tag), "sta_retry_%u", (unsigned)s_staBeginRetryCount);
            Serial.printf("[WiFiProv] STA stuck DISCONNECTED reason=0, retry begin %u/%u\n",
                          (unsigned)s_staBeginRetryCount, (unsigned)kStaBeginMaxExtraRetries);
            (void)applyDeferredSoftApAlign(false, "stuck_retry");
            staBeginNow(tag);
        }

        /* AUTH_EXPIRE / 握手超时：跨信道常见；有限次重试，优先无客户端时对齐 SoftAP */
        if (!staHasValidIp() && !s_stopRequested && isChannelSensitiveDisconnectReason(discReason) &&
            WiFi.status() != WL_CONNECTED && elapsed >= 1800 && s_authRetryCount < kAuthRetryMax &&
            (s_lastAuthRetryMs == 0 || (millis() - s_lastAuthRetryMs) >= kAuthRetryIntervalMs)) {
            s_authRetryCount++;
            s_lastAuthRetryMs = millis();
            const bool forceAlign = (s_authRetryCount >= kAuthRetryMax && s_deferredAlignChannel > 0);
            (void)applyDeferredSoftApAlign(forceAlign, forceAlign ? "auth_force" : "auth_retry");
            clearWifiDisconnectReason();
            s_lastLoggedDiscReason = 255;
            char tag[28];
            snprintf(tag, sizeof(tag), "sta_auth_%u", (unsigned)s_authRetryCount);
            Serial.printf("[WiFiProv] STA auth/handshake fail reason=%u, retry begin %u/%u force_align=%d\n",
                          (unsigned)discReason, (unsigned)s_authRetryCount, (unsigned)kAuthRetryMax,
                          forceAlign ? 1 : 0);
            staBeginNow(tag);
            discReason = getWifiDisconnectReason();
        }

        if (staHasValidIp() && !s_stopRequested) {
            if (!s_staConnectOk) {
                s_staConnectOk = true;
                s_connectedPortalHoldStart = millis();
                strncpy(s_staConnectIp, WiFi.localIP().toString().c_str(), sizeof(s_staConnectIp) - 1);
                s_staConnectIp[sizeof(s_staConnectIp) - 1] = '\0';
                saveWifiCredentialsFromPending();
                W20_WIFI_LOG("[WiFiProv] STA ready, hold portal for phone status polls ip=%s\n", s_staConnectIp);
                /* 拿到 IP 即通知 UI 刷「已连接」，配网功能视为完成 */
                s_uiNotifyFp = UINT32_MAX;
                provNotifyUiIfChanged();
            }
            const unsigned long held = millis() - s_connectedPortalHoldStart;
            const bool pollsOk = s_connectedStatusServed >= kPortalHoldMinStatusPolls;
            const bool minTimeOk = held >= kPortalHoldMinMs;
            const bool afterPollOk = pollsOk && held >= kPortalHoldAfterPollMs;
            const bool maxTimeOk = held >= kPortalHoldMaxMs;
            const bool absMaxOk = held >= kPortalHoldAbsMaxMs;
            const int apClients = WiFi.softAPgetStationNum();
            /* 配网成功：手机已看到 connected（或最短 hold）后立刻拆 SoftAP，切纯 STA */
            const bool canTearDown =
                afterPollOk || (minTimeOk && pollsOk) || absMaxOk || (maxTimeOk && (pollsOk || apClients == 0));
            if (!canTearDown) {
                if (maxTimeOk && !pollsOk && apClients > 0 && (held / 3000) != ((held - 50) / 3000)) {
                    W20_WIFI_LOG("[WiFiProv] portal hold wait status polls served=%u clients=%d held=%lums\n",
                                  (unsigned)s_connectedStatusServed, apClients, held);
                }
                provNotifyUiIfChanged();
            } else {
                W20_WIFI_LOG("[WiFiProv] portal hold done held=%lums status_served=%u clients=%d → STA only\n", held,
                              (unsigned)s_connectedStatusServed, apClients);
                resetConnectProgressFlags();
                /* 必须先解除 hold-offline：否则 teardown 会 WiFi.disconnect 刚配上的 STA。 */
                s_userHoldOffline = false;
                WiFi.setAutoReconnect(false);
                needReconnect = false;

                /* 停门户 HTTP/DNS；优先保留已拿到的 STA 关联，避免 WIFI_OFF→Reason8→重连。
                 * 下次再起 SoftAP 仍由 prepareWifiRadioForAp() 做 WIFI_OFF 清残留。 */
                s_http.stop();
                delay(30);
                s_dns.stop();
                s_dnsRunning = false;
                deauthSoftApClients();
                if (WiFi.softAPgetStationNum() > 0) {
                    delay(200);
                }
                const bool hadStaIp = staHasValidIp();
                const wifi_mode_t modeBefore = WiFi.getMode();
                s_running = false;
                s_softapUp = false;
                s_softapBoostPending = false;

                WiFi.softAPdisconnect(false);
                delay(80);

                bool keptSta = false;
                if (hadStaIp && (modeBefore == WIFI_AP_STA || modeBefore == WIFI_STA) &&
                    WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
                    WiFi.mode(WIFI_STA);
                    esp_wifi_set_mode(WIFI_MODE_STA);
                    delay(50);
                    if (staHasValidIp()) {
                        keptSta = true;
                        W20_WIFI_LOGLN("[WiFiProv] SoftAP down, STA association kept (no WIFI_OFF)");
                    }
                }

                if (!keptSta) {
                    /* 无有效 STA：干净 WIFI_OFF 再 begin（兼容纯 AP 配网路径） */
                    WiFi.mode(WIFI_OFF);
                    delay(200);
                    W20_WIFI_LOGLN("[WiFiProv] SoftAP fully off (WIFI_OFF) before STA restore");
                    WiFi.mode(WIFI_STA);
                    delay(80);
                    configureStaLikeMeshtasticBoot();
                    if (config.network.wifi_ssid[0]) {
                        const char *psk = config.network.wifi_psk[0] ? config.network.wifi_psk : nullptr;
                        W20_WIFI_LOG("[WiFiProv] STA re-begin after SoftAP teardown ssid=%s\n",
                                      config.network.wifi_ssid);
                        WiFi.begin(config.network.wifi_ssid, psk);
                    }
                } else {
                    configureStaLikeMeshtasticBoot();
                }
                WiFi.setAutoReconnect(true);

                s_state = UI_WIFI_PROV_CONNECTED;
                s_connectFailed = false;
                s_connectPending = false;
                {
                    const String ipNow = WiFi.localIP().toString();
                    const char *ipLog =
                        (ipNow != "0.0.0.0") ? ipNow.c_str() : (s_staConnectIp[0] ? s_staConnectIp : "0.0.0.0");
                    W20_WIFI_LOG("[WiFiProv] Connected, IP %s rssi=%d kept_sta=%d\n", ipLog, WiFi.RSSI(),
                                  keptSta ? 1 : 0);
                }

                /* 等 STA IP 稳定再清堆，避免 begin 过程中抢射频 */
                if (!keptSta) {
                    const unsigned long wait0 = millis();
                    while (!staHasValidIp() && (millis() - wait0) < 8000u) {
                        delay(50);
                    }
                }
                ui_wifi_provisioning_reclaim_heap();
                /* 强制通知 UI：配网完成，立刻刷已连接页 */
                s_uiNotifyFp = UINT32_MAX;
                provNotifyUiIfChanged();
                provLogStaSnapshot("connected_ok");
#if !MESHTASTIC_EXCLUDE_WEBSERVER
                if (!ui_wifi_menu_session_active()) {
                    restartWebServerIfNeeded();
                }
#endif
            }
        } else if (isFatalWifiDisconnectReason(discReason) && elapsed >= 1500 && WiFi.status() != WL_CONNECTED &&
                   !(isChannelSensitiveDisconnectReason(discReason) && s_authRetryCount < kAuthRetryMax)) {
            /* 信道敏感原因留足 auth 重试；已关联 AP、DHCP 进行中时不应沿用瞬时 disc_reason */
            markConnectFailed("disc_reason");
        } else if (WiFi.status() == WL_CONNECT_FAILED && elapsed >= 1500 &&
                   (s_deferredAlignChannel <= 0 || s_authRetryCount >= kAuthRetryMax)) {
            markConnectFailed("sta_status");
        } else if (elapsed > 45000) {
            /* 超时前最后一次强制对齐 SoftAP（可能踢手机）再 begin 一次 */
            if (s_deferredAlignChannel > 0 && s_authRetryCount < kAuthRetryMax + 1) {
                s_authRetryCount = kAuthRetryMax + 1;
                (void)applyDeferredSoftApAlign(true, "timeout_force");
                clearWifiDisconnectReason();
                s_lastLoggedDiscReason = 255;
                s_connectStartMs = millis(); /* 给最后一次 begin 再留一段窗口 */
                staBeginNow("sta_timeout_align");
            } else {
                markConnectFailed("timeout");
            }
        }
    }

    provNotifyUiIfChanged();
}

static void provTask(void *param)
{
    (void)param;
    while (s_taskRun) {
        if (s_startRequested || s_running || s_state == UI_WIFI_PROV_CONNECTING || s_stopRequested) {
            provisioningWork();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_taskHandle = nullptr;
    vTaskDelete(nullptr);
}

static void ensureProvTask()
{
    s_taskRun = true;
    if (s_taskHandle != nullptr)
        return;
    xTaskCreate(provTask, "wifiProv", 8192, nullptr, 1, &s_taskHandle);
}

/** SoftAP 已停、仅 STA：释放门户占用的堆，供 OTA / 其它功能使用。 */
void ui_wifi_provisioning_reclaim_heap(void)
{
    /* SoftAP/连接进行中不回收，避免掐正在用的门户 */
    if (s_running || s_softapUp || s_startRequested || s_stopRequested || s_pendingStartAfterStop ||
        s_connectPending || s_state == UI_WIFI_PROV_STARTING || s_state == UI_WIFI_PROV_CONNECTING ||
        s_state == UI_WIFI_PROV_AP) {
        W20_WIFI_LOG("[WiFiProv] reclaim skipped (busy state=%d running=%d)\n", (int)s_state,
                          s_running ? 1 : 0);
        return;
    }

    const uint32_t heap0 = ESP.getFreeHeap();

    s_http.stop();
    s_dns.stop();
    s_dnsRunning = false;

    /* Arduino String 无 swap：先清空再赋小串，尽量释放大块 scan JSON */
    s_scanJsonCache = String();
    s_scanJsonCache = "{\"status\":\"ok\",\"scanning\":false,\"data\":[]}";
    s_lastScanMs = 0;
    s_scanInProgress = false;
    s_ssidChCount = 0;
    WiFi.scanDelete();

#if !MESHTASTIC_EXCLUDE_WEBSERVER
    /* 菜单/OTA 期间不要占堆跑 Meshtastic Web/SSL；离开 WiFi 菜单再 restart */
    stopWebServer();
#endif

    /* 停空闲 wifiProv（8KB 栈）；下次 Config start → ensureProvTask。
     * 若本函数在 provTask 内调用：只置 s_taskRun=false，勿自等退出（会死锁）。 */
    if (s_taskHandle != nullptr && !s_running && !s_startRequested && !s_stopRequested) {
        const bool self = (xTaskGetCurrentTaskHandle() == s_taskHandle);
        s_taskRun = false;
        if (!self) {
            const unsigned long t0 = millis();
            while (s_taskHandle != nullptr && (millis() - t0) < 800u) {
                delay(10);
            }
            if (s_taskHandle != nullptr) {
                W20_WIFI_LOGLN("[WiFiProv] reclaim: provTask still exiting");
            }
        }
    }

    Serial.printf("[WiFiProv] reclaim heap %u -> %u\n", (unsigned)heap0, (unsigned)ESP.getFreeHeap());
}

bool ui_wifi_provisioning_start(bool force_reconfig)
{
    /* 停 SoftAP 过程中再请求启动：排队等 teardown 后起纯 AP */
    if (s_stopRequested) {
        s_pendingStartAfterStop = true;
        s_keepStaOnStart = false;
        meshApSsidFromMac(s_apSsid, sizeof(s_apSsid));
        s_state = UI_WIFI_PROV_STARTING;
        provNotifyUiIfChanged();
        W20_WIFI_LOG("[WiFiProv] start queued (stop in progress): %s force=%d\n", s_apSsid,
                      force_reconfig ? 1 : 0);
        return true;
    }
    if (s_running || s_startRequested || s_pendingStartAfterStop)
        return true;
    if (s_state == UI_WIFI_PROV_CONNECTING)
        return true;

    char ip[24] = {0};
    bool staLinked = false;
    if (WiFi.status() == WL_CONNECTED) {
        ui_wifi_get_ip(ip, sizeof(ip));
        staLinked = ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0;
    }
    if (!force_reconfig && staLinked) {
        s_state = UI_WIFI_PROV_CONNECTED;
        return true;
    }

    /* Strategy A: dedicated AP for portal — never AP_STA hybrid (keep_sta=0). */
    (void)staLinked;
    s_keepStaOnStart = false;
    meshApSsidFromMac(s_apSsid, sizeof(s_apSsid));

    s_connectFailed = false;
    resetConnectProgressFlags();
    ensureProvTask();

    s_startRequested = true;
    s_state = UI_WIFI_PROV_STARTING;
    provNotifyUiIfChanged();
    W20_WIFI_LOG("[WiFiProv] start requested: %s force=%d keep_sta=%d\n", s_apSsid, force_reconfig ? 1 : 0,
                  s_keepStaOnStart ? 1 : 0);
    return true;
}

void ui_wifi_provisioning_request_stop(void)
{
    if (s_state == UI_WIFI_PROV_IDLE && !s_running && !s_startRequested && !s_stopRequested &&
        !s_pendingStartAfterStop) {
        return;
    }

    /* Config 在「已连接」时只置 CONNECTED、不起 SoftAP：直接 IDLE，避免 OTA 傻等 provTask 8s */
    if (!s_running && !s_softapUp && !s_dnsRunning && !s_startRequested && !s_connectPending &&
        (s_state == UI_WIFI_PROV_CONNECTED || s_state == UI_WIFI_PROV_FAILED ||
         s_state == UI_WIFI_PROV_IDLE)) {
        s_stopRequested = false;
        s_pendingStartAfterStop = false;
        s_connectPending = false;
        s_startRequested = false;
        s_state = UI_WIFI_PROV_IDLE;
        s_uiNotifyFp = UINT32_MAX;
        W20_WIFI_LOGLN("[WiFiProv] stop fast-path (no SoftAP portal)");
        return;
    }

    s_startRequested = false;
    s_pendingStartAfterStop = false;
    s_connectPending = false;
    s_stopRequested = true;
    ensureProvTask();
}

bool ui_wifi_provisioning_wait_stopped(unsigned timeout_ms)
{
    if (s_state == UI_WIFI_PROV_IDLE && !s_running && !s_startRequested && !s_stopRequested &&
        !s_pendingStartAfterStop) {
        return true;
    }
    ui_wifi_provisioning_request_stop();
    const unsigned long t0 = millis();
    while ((s_stopRequested || s_running || s_startRequested || s_pendingStartAfterStop ||
            s_state != UI_WIFI_PROV_IDLE) &&
           (millis() - t0) < timeout_ms) {
        delay(20);
    }
    return s_state == UI_WIFI_PROV_IDLE && !s_running && !s_stopRequested && !s_startRequested &&
           !s_pendingStartAfterStop;
}

void ui_wifi_provisioning_stop(void)
{
    if (s_state == UI_WIFI_PROV_IDLE && !s_running && !s_startRequested && !s_stopRequested) {
        return;
    }

    ui_wifi_provisioning_request_stop();

    /* 只等 provTask 做 teardown；禁止在调用线程 force teardown（与任务并发会卡死 WiFi）。 */
    if (!ui_wifi_provisioning_wait_stopped(5000)) {
        Serial.println("[WiFiProv] stop slow/incomplete — leaving SoftAP teardown to provTask");
        /* 仅当配网任务已死才兜底，避免双线程抢 WiFi 锁 */
        if (s_taskHandle == nullptr && (s_running || s_softapUp || s_stopRequested)) {
            Serial.println("[WiFiProv] provTask gone — emergency teardown");
            s_stopRequested = false;
            s_connectFailed = false;
            const bool keepSta = (WiFi.status() == WL_CONNECTED);
            s_running = false;
            ui_wifi_force_ap_teardown(keepSta);
            if (keepSta) {
                needReconnect = false;
            }
            s_state = UI_WIFI_PROV_IDLE;
            s_uiNotifyFp = UINT32_MAX;
        }
    }
}

ui_wifi_prov_state_t ui_wifi_provisioning_get_state(void)
{
    return s_state;
}

void ui_wifi_provisioning_get_ap_ssid(char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    strncpy(buf, s_apSsid, len - 1);
    buf[len - 1] = '\0';
}

void ui_wifi_provisioning_get_status_text(char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    switch (s_state) {
    case UI_WIFI_PROV_STARTING:
        snprintf(buf, len, "AP starting...");
        break;
    case UI_WIFI_PROV_AP:
        if (s_connectFailed) {
            snprintf(buf, len, "%s", PROV_CONNECT_FAIL_MSG);
        } else {
            snprintf(buf, len, "AP: use phone browser");
        }
        break;
    case UI_WIFI_PROV_CONNECTING:
        snprintf(buf, len, "Connecting...");
        break;
    case UI_WIFI_PROV_CONNECTED:
        snprintf(buf, len, "OK %s", WiFi.localIP().toString().c_str());
        break;
    case UI_WIFI_PROV_FAILED:
        snprintf(buf, len, "AP start failed");
        break;
    default:
        snprintf(buf, len, "Starting...");
        break;
    }
}

int ui_wifi_provisioning_get_client_count(void)
{
    return WiFi.softAPgetStationNum();
}

bool ui_wifi_provisioning_connect_failed(void)
{
    return s_connectFailed;
}

bool ui_wifi_provisioning_get_last_sta_ip(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return false;
    }
    buf[0] = '\0';
    if (s_staConnectIp[0] == '\0') {
        return false;
    }
    strncpy(buf, s_staConnectIp, len - 1);
    buf[len - 1] = '\0';
    return true;
}

void ui_wifi_user_hold_offline_set(bool hold)
{
    s_userHoldOffline = hold;
    if (hold) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        w20_wifi_abort_sta_reconnect();
#endif
        WiFi.setAutoReconnect(false);
        /* Disconnect 换网：清缓存 IP / CONNECTED，避免 UI 仍显示旧路由器 */
        s_staConnectIp[0] = '\0';
        s_staConnectOk = false;
        if (s_state == UI_WIFI_PROV_CONNECTED) {
            s_state = UI_WIFI_PROV_IDLE;
        }
        W20_WIFI_LOGLN("[WiFiProv] user hold offline ON (re-provision)");
    } else {
        W20_WIFI_LOGLN("[WiFiProv] user hold offline OFF");
    }
}

bool ui_wifi_user_hold_offline(void)
{
    return s_userHoldOffline;
}

#else /* !ARDUINO_ARCH_ESP32 */

bool ui_wifi_provisioning_active(void)
{
    return false;
}
bool ui_wifi_provisioning_in_sta_handoff(void)
{
    return false;
}
bool ui_wifi_provisioning_start(bool force_reconfig)
{
    (void)force_reconfig;
    return false;
}
bool ui_wifi_softap_begin(bool keep_sta, bool ota_ap_only)
{
    (void)keep_sta;
    (void)ota_ap_only;
    return false;
}
void ui_wifi_softap_end(bool keep_sta)
{
    (void)keep_sta;
}
bool ui_wifi_softap_is_up(void)
{
    return false;
}
void ui_wifi_softap_get_ssid(char *buf, size_t len)
{
    if (buf && len)
        buf[0] = '\0';
}
void ui_wifi_pause_ble_for_radio(void) {}
void ui_wifi_provisioning_request_stop(void) {}
bool ui_wifi_provisioning_wait_stopped(unsigned timeout_ms)
{
    (void)timeout_ms;
    return true;
}
void ui_wifi_provisioning_reclaim_heap(void) {}
void ui_wifi_provisioning_stop(void) {}
void ui_wifi_force_ap_teardown(bool keep_sta)
{
    (void)keep_sta;
}
void ui_wifi_provisioning_set_ui_callback(ui_wifi_prov_ui_cb_t cb)
{
    (void)cb;
}
ui_wifi_prov_state_t ui_wifi_provisioning_get_state(void)
{
    return UI_WIFI_PROV_IDLE;
}
void ui_wifi_provisioning_get_ap_ssid(char *buf, size_t len)
{
    if (buf && len)
        buf[0] = '\0';
}
void ui_wifi_provisioning_get_status_text(char *buf, size_t len)
{
    if (buf && len)
        strncpy(buf, "N/A", len - 1);
}
int ui_wifi_provisioning_get_client_count(void)
{
    return 0;
}
bool ui_wifi_provisioning_connect_failed(void)
{
    return false;
}
bool ui_wifi_provisioning_get_last_sta_ip(char *buf, size_t len)
{
    if (buf && len)
        buf[0] = '\0';
    return false;
}
void ui_wifi_user_hold_offline_set(bool hold)
{
    (void)hold;
}
bool ui_wifi_user_hold_offline(void)
{
    return false;
}

#endif
