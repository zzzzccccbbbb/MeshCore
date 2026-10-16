#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "w20_wifi_ota.h"
#include "w20_auto_off.h"
#include "w20_dbg_log.h"
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"
#include "ui_deckpro_port.h"
#include "ui_wifi_provision.h"
#include "mesh/wifi/WiFiAPClient.h"
#include "configuration.h"
#include "main.h"
#include "peripheral.h"

/* 大文件 OTA 需更长 TCP 等待；默认 5s 会在 esp_ota 写 Flash 时导致 PC 断连 */
#ifndef HTTP_MAX_SEND_WAIT
#define HTTP_MAX_SEND_WAIT 120000
#endif
#ifndef HTTP_MAX_DATA_WAIT
#define HTTP_MAX_DATA_WAIT 120000
#endif
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "w20_wifi_ble_coex.h"
#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include <cstring>

/** W20 16MB 分区表 app0/app1 各 0x480000；旧 4MB 表 ota_1 仅 0xA0000 会导致写满后失败 */
static constexpr size_t kW20OtaSlotRecommendedBytes = 0x480000;

#ifndef W20_WIFI_OTA_PORT_STA
#define W20_WIFI_OTA_PORT_STA 8080
#endif
/** 开 HTTP 前参考阈值；过低只做短等回收，不再拒绝启动。 */
#ifndef W20_WIFI_OTA_MIN_HEAP
#define W20_WIFI_OTA_MIN_HEAP 40000u
#endif

static WebServer s_httpSta(W20_WIFI_OTA_PORT_STA);
static WebServer *s_http = nullptr;
static bool s_running = false;
static bool s_routesStaRegistered = false;
static bool s_uploadFailed = false;
static bool s_magicPending = true;
static bool s_epdFlushPausedForUpload = false;
static TaskHandle_t s_prepareTask = nullptr;
static volatile bool s_otaSessionReady = false;
static volatile bool s_otaPrepareFailed = false;
/** 离开 OTA 页时置位，打断 STA 重连等待 */
static volatile bool s_abortRequested = false;
static unsigned long s_prepRetryAfterMs = 0;
static uint8_t s_prepCreateFailCount = 0;

static esp_ota_handle_t s_otaHandle = 0;
static const esp_partition_t *s_updatePart = nullptr;
static size_t s_written = 0;
static int s_progress = -1;
static char s_status[96] = "Idle";

static void setStatus(const char *msg)
{
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    W20_WIFI_LOG("[W20WifiOta] %s\n", s_status);
}

static void otaStartPrepareAsync(void);

static void handleOtaProgress()
{
    char buf[192];
    const bool preparing = (s_prepareTask != nullptr) ||
                           (!s_otaSessionReady && s_running && (!s_otaPrepareFailed || s_prepRetryAfterMs != 0));
    snprintf(buf, sizeof(buf),
             "{\"progress\":%d,\"status\":\"%s\",\"failed\":%s,\"done\":%s,\"preparing\":%s}",
             s_progress < 0 ? 0 : s_progress, s_status, s_uploadFailed ? "true" : "false",
             (s_progress >= 100 && !s_uploadFailed) ? "true" : "false", preparing ? "true" : "false");
    s_http->sendHeader("Cache-Control", "no-cache");
    s_http->send(200, "application/json", buf);
}

static void sendOtaHomeRedirect()
{
    s_http->sendHeader("Location", "/ota", true);
    s_http->send(302, "text/plain", "");
}

static const char OTA_OK_HTML[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>w20 MESHnology</title><style>body{font-family:system-ui,sans-serif;background:#0c1117;color:#3dd68c;text-align:center;padding:3rem 1rem}"
    "h1{font-size:1.25rem}</style></head><body><h1>Flash complete</h1><p style=\"color:#8b9cb3\">Device rebooting...</p></body></html>";

static const char OTA_ERR_HTML[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>w20 MESHnology</title><style>body{font-family:system-ui,sans-serif;background:#0c1117;color:#ff7b7b;text-align:center;padding:3rem 1rem}"
    "h1{font-size:1.25rem}</style></head><body><h1>Upgrade failed</h1><p style=\"color:#8b9cb3\">Check firmware filename and serial log</p></body></html>";

/**
 * 轻量 OTA 页（PROGMEM + 分块 sendContent），避免低堆下拼 5KB String 卡 10s。
 */
static void otaSendProgmemChunks(PGM_P page)
{
    const size_t len = strlen_P(page);
    {
        WiFiClient c = s_http->client();
        c.setTimeout(2000);
    }
    s_http->setContentLength(len);
    s_http->sendHeader("Cache-Control", "no-cache");
    s_http->send(200, "text/html", "");
    char buf[256];
    for (size_t off = 0; off < len;) {
        size_t n = len - off;
        if (n > sizeof(buf))
            n = sizeof(buf);
        memcpy_P(buf, page + off, n);
        s_http->sendContent(buf, n);
        off += n;
        delay(0);
    }
}

static const char OTA_PAGE_READY[] PROGMEM =
    "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>w20 MESHnology · OTA</title><style>"
    ":root{--bg:#0c1117;--card:#151d28;--t:#e6edf5;--m:#8b9cb3;--a:#3dd68c;--b:#263244;--e:#ff7b7b}"
    "*{box-sizing:border-box}"
    "body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--t);padding:16px}"
    ".w{max-width:420px;margin:0 auto}"
    ".h{display:flex;gap:10px;align-items:center;margin:0 0 14px}"
    ".logo{width:36px;height:36px;border-radius:10px;background:var(--a);display:flex;align-items:center;"
    "justify-content:center;font-size:9px;font-weight:800;color:#0c1117;flex-shrink:0}"
    "h1{margin:0;font-size:15px}p.s{margin:2px 0 0;font-size:12px;color:var(--m)}"
    ".c{background:var(--card);border:1px solid var(--b);border-radius:14px;padding:14px;margin:0 0 10px}"
    ".hi{font-size:12px;color:var(--m);margin:0 0 12px;line-height:1.45}"
    ".zone{display:block;width:100%;border:2px dashed var(--b);border-radius:10px;padding:18px 12px;"
    "text-align:center;margin:0 0 10px;cursor:pointer;background:#0f1620}"
    ".zone:hover,.zone.drag{border-color:var(--a);background:rgba(61,214,140,.06)}"
    ".zone input{display:none}"
    ".zone .lab{display:block;font-size:14px;color:var(--m);line-height:1.4}"
    ".zone .fn{margin-top:8px;font-size:13px;color:var(--a);word-break:break-all;min-height:1.2em}"
    "button{width:100%;padding:12px;margin:0;border-radius:9px;border:0;background:var(--a);color:#0c1117;"
    "font-size:15px;font-weight:600}button:disabled{opacity:.45}"
    ".prog{display:none;margin-top:12px}.prog.on{display:block}"
    ".bar{height:8px;background:#0f1620;border-radius:4px;overflow:hidden;border:1px solid var(--b)}"
    ".fill{height:100%;width:0;background:var(--a);border-radius:4px}"
    ".pct{display:flex;justify-content:space-between;font-size:12px;color:var(--m);margin-top:6px}"
    ".st{margin-top:10px;min-height:40px;padding:10px 12px;border-radius:9px;background:#0f1620;"
    "border:1px solid var(--b);font-size:13px;line-height:1.4}"
    ".st.ok{border-color:rgba(61,214,140,.35);color:var(--a)}"
    ".st.er{border-color:rgba(255,123,123,.35);color:var(--e)}"
    "code{font-size:11px;background:#0f1620;padding:1px 5px;border-radius:4px}"
    "</style></head><body><div class=w>"
    "<div class=h><div class=logo>W20</div><div><h1>w20 MESHnology</h1><p class=s>Firmware Update</p></div></div>"
    "<div class=c>"
    "<p class=hi>Upload an OTA firmware file. The filename must contain <code>w20</code>, "
    "e.g. <code>w20-firmware-ota.bin</code>.</p>"
    "<form id=f method=POST action=/ota/upload enctype=multipart/form-data>"
    "<label class=zone id=zone><input type=file id=file name=firmware accept=.bin>"
    "<span class=lab>Tap to choose a file<br>or drag and drop here</span>"
    "<div class=fn id=fname></div></label>"
    "<button type=submit id=btn disabled>Upload &amp; Upgrade</button>"
    "<div class=prog id=prog><div class=bar><div class=fill id=fill></div></div>"
    "<div class=pct><span id=phase>Ready</span><span id=pct>0%</span></div></div>"
    "</form>"
    "<div class=st id=msg>Waiting for firmware file</div>"
    "</div></div><script>"
    "var sel=null,T=null;"
    "function msg(t,c){var m=document.getElementById('msg');m.className='st'+(c?' '+c:'');m.textContent=t;}"
    "function prog(p,ph){document.getElementById('prog').classList.add('on');"
    "document.getElementById('fill').style.width=p+'%';document.getElementById('pct').textContent=p+'%';"
    "if(ph)document.getElementById('phase').textContent=ph;}"
    "function okName(n){return n&&n.toLowerCase().indexOf('w20')>=0;}"
    "function pick(f){if(!f)return;sel=f;document.getElementById('fname').textContent=f.name;"
    "if(!okName(f.name)){msg('Filename must contain w20','er');document.getElementById('btn').disabled=1;}"
    "else{msg('Selected: '+f.name);document.getElementById('btn').disabled=0;}}"
    "var zone=document.getElementById('zone'),inp=document.getElementById('file');"
    "inp.onchange=function(){pick(inp.files[0]);};"
    "zone.ondragover=function(e){e.preventDefault();zone.classList.add('drag');};"
    "zone.ondragleave=function(){zone.classList.remove('drag');};"
    "zone.ondrop=function(e){e.preventDefault();zone.classList.remove('drag');pick(e.dataTransfer.files[0]);};"
    "function poll(){fetch('/ota/progress').then(function(r){return r.json();}).then(function(j){"
    "if(j.progress>=0)prog(j.progress,j.failed?'Failed':(j.done?'Writing done':'Flashing'));"
    "if(j.failed){clearInterval(T);T=null;msg(j.status||'Upgrade failed','er');"
    "document.getElementById('btn').disabled=0;}"
    "if(j.done){clearInterval(T);T=null;prog(100,'Done');msg('Flash complete — rebooting','ok');}"
    "}).catch(function(){});}"
    "document.getElementById('f').onsubmit=function(e){"
    "if(!sel||!okName(sel.name)){e.preventDefault();msg('Please select a valid firmware file','er');return;}"
    "e.preventDefault();document.getElementById('btn').disabled=1;"
    "msg('Uploading firmware…');prog(0,'Uploading');"
    "var fd=new FormData();fd.append('firmware',sel);var x=new XMLHttpRequest();"
    "x.open('POST','/ota/upload');"
    "x.upload.onprogress=function(ev){if(ev.lengthComputable){"
    "prog(Math.min(99,Math.round(ev.loaded*100/ev.total)),'Uploading');}};"
    "x.onload=function(){if(x.status===200){prog(100,'Done');msg('Flash complete — rebooting','ok');}"
    "else{msg('Upload or flash failed','er');document.getElementById('btn').disabled=0;}};"
    "x.onerror=function(){msg('Network error. Please retry.','er');document.getElementById('btn').disabled=0;};"
    "T=setInterval(poll,500);x.send(fd);};"
    "</script></body></html>";

static const char OTA_PAGE_PREP[] PROGMEM =
    "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<meta http-equiv=refresh content=2><title>w20 OTA</title>"
    "<style>body{margin:0;font-family:system-ui;background:#0c1117;color:#8b9cb3;padding:24px;text-align:center}"
    "h1{color:#e6edf5;font-size:16px}</style></head><body>"
    "<h1>Preparing flash slot…</h1><p>This page refreshes automatically.</p></body></html>";

static const char OTA_PAGE_FAIL[] PROGMEM =
    "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>w20 OTA</title>"
    "<style>body{margin:0;font-family:system-ui;background:#0c1117;color:#ff7b7b;padding:24px;text-align:center}"
    "h1{font-size:16px}a{color:#3dd68c}</style></head><body>"
    "<h1>Prepare failed</h1><p><a href=/ota>Tap here to retry</a></p></body></html>";

static void handleOtaGet()
{
    const bool ready = s_otaSessionReady && s_otaHandle;
    const uint32_t t0 = millis();
    W20_WIFI_LOG("[W20WifiOta] HTTP GET /ota heap=%u ready=%d prep=%d fail=%d\n",
                  (unsigned)ESP.getFreeHeap(), ready ? 1 : 0,
                  (!ready && !s_otaPrepareFailed && (s_prepareTask != nullptr || (!s_otaSessionReady && s_running))) ? 1 : 0,
                  s_otaPrepareFailed ? 1 : 0);

    PGM_P page = OTA_PAGE_READY;
    if (!ready) {
        page = s_otaPrepareFailed ? OTA_PAGE_FAIL : OTA_PAGE_PREP;
    }
    otaSendProgmemChunks(page);
    W20_WIFI_LOG("[W20WifiOta] GET /ota HTML sent ms=%lu\n", (unsigned long)(millis() - t0));
    if (!s_otaSessionReady && s_prepareTask == nullptr && !s_otaHandle) {
        otaStartPrepareAsync();
    }
}

/** 上传写 Flash 期间暂停 EPD；勿每块都狂 yield，否则进度条极慢。 */
static void otaUploadYield(void)
{
    static uint8_t n = 0;
    esp_task_wdt_reset();
    /* 约每 16 个 upload chunk 让出一次（旧逻辑每块 yield×12+1ms，数 MB 固件会慢很多） */
    if ((++n & 0x0f) == 0) {
        yield();
    }
}

static void otaAbortPreparedSession(void)
{
    if (s_otaHandle) {
        esp_ota_abort(s_otaHandle);
        s_otaHandle = 0;
    }
    s_otaSessionReady = false;
    s_otaPrepareFailed = false;
    s_prepRetryAfterMs = 0;
}

static void otaSetEpdFlushPaused(bool pause)
{
    if (pause) {
        if (!s_epdFlushPausedForUpload) {
            factory_pause_flush_timer();
            s_epdFlushPausedForUpload = true;
            W20_WIFI_LOGLN("[W20WifiOta] EPD flush paused for upload");
        }
        return;
    }
    if (s_epdFlushPausedForUpload) {
        factory_resume_flush_timer();
        s_epdFlushPausedForUpload = false;
        W20_WIFI_LOGLN("[W20WifiOta] EPD flush resumed");
    }
}

static void otaLogPartition(const esp_partition_t *part, const char *role)
{
    if (!part) {
        Serial.printf("[W20WifiOta] %s partition: (null)\n", role);
        return;
    }
    W20_WIFI_LOG("[W20WifiOta] %s partition: label=%s subtype=%u addr=0x%06x size=%u (%u KB)\n", role, part->label,
                  (unsigned)part->subtype, (unsigned)part->address, (unsigned)part->size, (unsigned)(part->size / 1024));
}

/** 浏览器 OTA 只在 app0/app1 间切换（旧分区表上的 wifiota/ota_2 槽会被拒绝）。 */
static const esp_partition_t *otaPickFirmwareUpdatePartition(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *app0 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    const esp_partition_t *app1 =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);

    if (!app0 && !app1) {
        Serial.println("[W20WifiOta] WARN: no ota_0/ota_1; fallback esp_ota_get_next_update_partition");
        return esp_ota_get_next_update_partition(nullptr);
    }

    if (running && app0 && running->address == app0->address) {
        if (app1) {
            W20_WIFI_LOGLN("[W20WifiOta] pick target app1 (running app0)");
            return app1;
        }
    } else if (running && app1 && running->address == app1->address) {
        if (app0) {
            W20_WIFI_LOGLN("[W20WifiOta] pick target app0 (running app1)");
            return app0;
        }
    } else {
        const esp_partition_t *pick = nullptr;
        if (app0 && (!running || running->address != app0->address)) {
            pick = app0;
        }
        if (app1 && (!running || running->address != app1->address)) {
            if (!pick || app1->size > pick->size) {
                pick = app1;
            }
        }
        if (pick) {
            W20_WIFI_LOG("[W20WifiOta] pick target %s (running not in app0/app1)\n", pick->label);
            return pick;
        }
    }

    return app0 ? app0 : app1;
}

static void otaLogWriteContext(size_t chunkLen, esp_err_t err)
{
    otaLogPartition(esp_ota_get_running_partition(), "running");
    otaLogPartition(s_updatePart, "target");
    if (err != ESP_OK) {
        Serial.printf("[W20WifiOta] written=%u chunk=%u err=%s (0x%x)\n", (unsigned)s_written, (unsigned)chunkLen,
                      esp_err_to_name(err), (unsigned)err);
    } else {
        W20_WIFI_LOG("[W20WifiOta] written=%u chunk=%u\n", (unsigned)s_written, (unsigned)chunkLen);
    }
    if (s_updatePart && s_written + chunkLen > s_updatePart->size) {
        Serial.println("[W20WifiOta] hint: firmware larger than target OTA slot — full reflash with "
                       "partition-table-16MB.csv (erase + factory.bin)");
    } else if (s_updatePart && s_updatePart->size < kW20OtaSlotRecommendedBytes) {
        Serial.printf("[W20WifiOta] hint: target slot only %u bytes (W20 needs ~%u per app slot)\n",
                      (unsigned)s_updatePart->size, (unsigned)kW20OtaSlotRecommendedBytes);
    }
}

static void otaSetFailStatus(const char *shortMsg, esp_err_t err)
{
    otaSetEpdFlushPaused(false);
    s_uploadFailed = true;
    if (err != ESP_OK) {
        snprintf(s_status, sizeof(s_status), "%s (%s)", shortMsg, esp_err_to_name(err));
    } else {
        strncpy(s_status, shortMsg, sizeof(s_status) - 1);
        s_status[sizeof(s_status) - 1] = '\0';
    }
    /* 失败状态始终打串口，不受 W20_LOG_WIFI 控制 */
    Serial.printf("[W20WifiOta] %s\n", s_status);
}

/** 实际执行 esp_ota_begin；由异步任务或同步回退调用。返回是否就绪。 */
static bool otaPrepareSessionBody(void)
{
    s_updatePart = otaPickFirmwareUpdatePartition();
    if (!s_updatePart) {
        otaSetFailStatus("No OTA partition", ESP_ERR_NOT_FOUND);
        s_otaPrepareFailed = true;
        return false;
    }

    otaLogPartition(esp_ota_get_running_partition(), "running");
    otaLogPartition(s_updatePart, "target");
    if (strcmp(s_updatePart->label, "wifiota") == 0 || s_updatePart->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_2) {
        otaSetFailStatus("Bad OTA slot", ESP_ERR_INVALID_ARG);
        s_otaPrepareFailed = true;
        return false;
    }

    setStatus("Preparing OTA...");
    W20_WIFI_LOGLN("[W20WifiOta] async esp_ota_begin (sequential) — no full-slot erase");
    otaSetEpdFlushPaused(true);
    /* OTA_WITH_SEQUENTIAL_WRITES：边写边擦，避免进页就擦 4.5MB 把整机卡死 */
    const esp_err_t beginErr = esp_ota_begin(s_updatePart, OTA_WITH_SEQUENTIAL_WRITES, &s_otaHandle);
    if (beginErr != ESP_OK) {
        otaSetEpdFlushPaused(false);
        otaLogWriteContext(0, beginErr);
        otaSetFailStatus("esp_ota_begin failed", beginErr);
        s_otaPrepareFailed = true;
        return false;
    }

    /* 会话就绪后仍停墨，直到 stop/失败；避免 OTA 页空闲时 EPD 抢 CPU/堆 */
    s_otaPrepareFailed = false;
    s_otaSessionReady = true;
    setStatus("Server ready");
    W20_WIFI_LOGLN("[W20WifiOta] OTA session ready (sequential writes)");
    return true;
}

static void otaPrepareTask(void *param)
{
    (void)param;
    (void)otaPrepareSessionBody();
    s_prepareTask = nullptr;
    vTaskDelete(nullptr);
}

static bool otaTryCreatePrepareTask(void)
{
    /* 堆紧张时逐步减小栈；短暂让出后重试 */
    static const uint32_t kStacks[] = {4096, 3072, 2048};
    for (unsigned i = 0; i < sizeof(kStacks) / sizeof(kStacks[0]); i++) {
        s_prepareTask = nullptr;
        if (xTaskCreate(otaPrepareTask, "otaPrep", kStacks[i], nullptr, 1, &s_prepareTask) == pdPASS) {
            W20_WIFI_LOG("[W20WifiOta] otaPrep task ok stack=%u\n", (unsigned)kStacks[i]);
            return true;
        }
        s_prepareTask = nullptr;
        Serial.printf("[W20WifiOta] otaPrep xTaskCreate fail stack=%u (heap=%u)\n", (unsigned)kStacks[i],
                      (unsigned)ESP.getFreeHeap());
        delay(30);
    }
    return false;
}

static void otaStartPrepareAsync(void)
{
    if (s_otaSessionReady || s_prepareTask != nullptr || s_otaHandle) {
        return;
    }
    s_otaPrepareFailed = false;
    s_prepRetryAfterMs = 0;
    if (otaTryCreatePrepareTask()) {
        s_prepCreateFailCount = 0;
        return;
    }

    s_prepCreateFailCount++;
    Serial.printf("[W20WifiOta] otaPrep create failed #%u — inline fallback\n", (unsigned)s_prepCreateFailCount);
    setStatus("Preparing OTA...");
    if (otaPrepareSessionBody()) {
        s_prepCreateFailCount = 0;
        return;
    }

    /* 同步 begin 也失败：稍后再从 loop 重试，避免永久装聋 */
    setStatus("OTA prep retry soon");
    s_otaPrepareFailed = true;
    s_prepRetryAfterMs = millis() + 1500;
    Serial.println("[W20WifiOta] OTA prep failed; will retry from loop");
}

static bool otaFilenameAllowed(const String &filename)
{
    if (filename.length() == 0) {
        return false;
    }
    int base = filename.lastIndexOf('/') + 1;
    const int bs = filename.lastIndexOf('\\') + 1;
    if (bs > base) {
        base = bs;
    }
    String name = filename.substring(base);
    name.toLowerCase();
    return name.indexOf("w20") >= 0;
}

static bool otaWritePayload(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return true;
    }
    const esp_err_t err = esp_ota_write(s_otaHandle, data, len);
    if (err != ESP_OK) {
        otaLogWriteContext(len, err);
        esp_ota_abort(s_otaHandle);
        otaSetFailStatus("Write failed", err);
        return false;
    }
    s_written += len;
    return true;
}

static void otaAbortWithStatus(const char *msg)
{
    if (s_otaHandle) {
        esp_ota_abort(s_otaHandle);
        s_otaHandle = 0;
    }
    otaSetFailStatus(msg, ESP_OK);
}

static void handleOtaUpload()
{
    HTTPUpload &upload = s_http->upload();

    if (upload.status == UPLOAD_FILE_START) {
        s_uploadFailed = false;
        s_magicPending = true;
        s_written = 0;
        s_progress = 0;
        setStatus("Receiving...");

        if (ui_wifi_provisioning_active()) {
            s_uploadFailed = true;
            setStatus("AP mode active");
            return;
        }

        if (!otaFilenameAllowed(upload.filename)) {
            s_uploadFailed = true;
            setStatus("Name need w20");
            Serial.printf("[W20WifiOta] reject filename: %s\n", upload.filename.c_str());
            return;
        }
        W20_WIFI_LOG("[W20WifiOta] accept filename: %s\n", upload.filename.c_str());

        if (s_prepareTask != nullptr) {
            s_uploadFailed = true;
            setStatus("OTA preparing, retry");
            Serial.println("[W20WifiOta] upload rejected: prepare still in progress");
            return;
        }
        if (!s_otaSessionReady || !s_otaHandle) {
            if (s_otaPrepareFailed || s_prepareTask == nullptr) {
                /* 上次失败或尚未启动：再试一次，让浏览器重传 */
                otaStartPrepareAsync();
            }
            s_uploadFailed = true;
            setStatus("OTA preparing, retry");
            return;
        }

        otaLogPartition(esp_ota_get_running_partition(), "running");
        otaLogPartition(s_updatePart, "target");
        if (s_updatePart && s_updatePart->size < kW20OtaSlotRecommendedBytes) {
            Serial.printf("[W20WifiOta] WARN: target slot %u bytes < recommended %u — upgrade may fail; "
                          "use partition-table-16MB.csv + full factory flash\n",
                          (unsigned)s_updatePart->size, (unsigned)kW20OtaSlotRecommendedBytes);
        }

        otaSetEpdFlushPaused(true);
        W20_WIFI_LOG("[W20WifiOta] OTA session ready, upload total (incl magic)=%u bytes\n",
                      (unsigned)upload.totalSize);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (s_uploadFailed) {
            return;
        }
        esp_task_wdt_reset();
        if (s_magicPending) {
            if (upload.currentSize < 1) {
                otaAbortWithStatus("Empty file");
                return;
            }
            if (upload.buf[0] != W20_WIFI_OTA_FILE_MAGIC) {
                Serial.printf("[W20WifiOta] bad magic 0x%02x want 0x%02x\n", upload.buf[0], W20_WIFI_OTA_FILE_MAGIC);
                otaAbortWithStatus("Bad file header");
                return;
            }
            s_magicPending = false;
            if (!otaWritePayload(upload.buf + 1, upload.currentSize - 1)) {
                return;
            }
            otaUploadYield();
        } else if (!otaWritePayload(upload.buf, upload.currentSize)) {
            return;
        }
        otaUploadYield();
        if (upload.totalSize > 1) {
            s_progress = (int)((s_written * 100) / (upload.totalSize - 1));
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (s_uploadFailed) {
            return;
        }
        if (s_magicPending) {
            otaAbortWithStatus("Bad file header");
            return;
        }
        const esp_err_t endErr = esp_ota_end(s_otaHandle);
        s_otaHandle = 0;
        if (endErr != ESP_OK) {
            otaLogWriteContext(0, endErr);
            otaSetFailStatus("esp_ota_end failed", endErr);
            return;
        }
        const esp_err_t bootErr = esp_ota_set_boot_partition(s_updatePart);
        if (bootErr != ESP_OK) {
            otaLogWriteContext(0, bootErr);
            otaSetFailStatus("set_boot failed", bootErr);
            return;
        }
        W20_WIFI_LOG("[W20WifiOta] flash OK written=%u target=%s\n", (unsigned)s_written,
                      s_updatePart ? s_updatePart->label : "?");
        s_progress = 100;
        setStatus("Success, rebooting");
        rebootAtMsec = millis() + 3000;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (s_otaHandle) {
            esp_ota_abort(s_otaHandle);
            s_otaHandle = 0;
        }
        otaSetFailStatus("Upload aborted", ESP_OK);
    }
}

static void handleOtaPostDone()
{
    if (s_uploadFailed) {
        s_http->send_P(500, "text/html", OTA_ERR_HTML);
    } else {
        s_http->send_P(200, "text/html", OTA_OK_HTML);
    }
}

static void registerOtaRoutes(WebServer &http)
{
    http.on("/ota", HTTP_GET, handleOtaGet);
    http.on("/ota/progress", HTTP_GET, handleOtaProgress);
    http.on("/", HTTP_GET, sendOtaHomeRedirect);
    http.on(
        "/ota/upload", HTTP_POST,
        []() { handleOtaPostDone(); },
        []() { handleOtaUpload(); });
}

static bool beginOtaHttpServer(void)
{
    if (!s_http) {
        return false;
    }
    w20_wifi_suspend_meshtastic_app_services();
    if (!s_routesStaRegistered) {
        registerOtaRoutes(s_httpSta);
        s_routesStaRegistered = true;
    }
    s_httpSta.begin();
    return true;
}

extern "C" {

static void ensureProvisioningStoppedForOta(void)
{
    if (ui_wifi_provisioning_get_state() == UI_WIFI_PROV_IDLE && !ui_wifi_provisioning_active()) {
        return;
    }
    if (!ui_wifi_provisioning_wait_stopped(3000)) {
        W20_WIFI_LOGLN("[W20WifiOta] provisioning still not idle");
    }
}

void w20_wifi_ota_abort_start(void)
{
    s_abortRequested = true;
}

bool w20_wifi_ota_prepare_sta(void)
{
    if (w20_deck_sleep_active()) {
        w20_deck_sleep_wake();
        w20_auto_off_kick();
    }

    ui_wifi_menu_power_on();

    if (ui_wifi_sta_has_valid_ip()) {
        /* SoftAP 刚拆完偶发 IP 抖 0：稳 600ms 再开 HTTP */
        const unsigned long t0 = millis();
        while ((millis() - t0) < 600u) {
            if (!ui_wifi_sta_has_valid_ip()) {
                break;
            }
            delay(50);
        }
        if (ui_wifi_sta_has_valid_ip()) {
            return true;
        }
        W20_WIFI_LOGLN("[W20WifiOta] STA IP lost during settle — reconnect");
    }

    /* 用户在 Config 点过 Disconnect：禁止 OTA 自动联网，提示去 Config 重配 */
    if (ui_wifi_user_hold_offline()) {
        setStatus("Use WIFI Config");
        W20_WIFI_LOGLN("[W20WifiOta] hold offline — refuse auto reconnect");
        return false;
    }

    if (!config.network.wifi_ssid[0]) {
        setStatus("Need WiFi first");
        LOG_WARN("W20 WiFi OTA: no saved SSID");
        return false;
    }

    setStatus("Connecting WiFi...");
    W20_WIFI_LOG("[W20WifiOta] STA not linked — reconnect ssid=%s mode=%d\n", config.network.wifi_ssid,
                  (int)WiFi.getMode());
    /* SoftAP 刚拆完时务必干净切到 STA 再 begin */
    WiFi.persistent(false);
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(150);
    }
    WiFi.disconnect(false, false);
    delay(80);
    const char *psk = config.network.wifi_psk[0] ? config.network.wifi_psk : NULL;
    ui_wifi_connect(config.network.wifi_ssid, psk);
    WiFi.setAutoReconnect(true);

    const unsigned long t0 = millis();
    unsigned lastLog = 0;
    while ((millis() - t0) < 20000) {
        if (s_abortRequested) {
            W20_WIFI_LOGLN("[W20WifiOta] reconnect aborted");
            setStatus("Cancelled");
            return false;
        }
        if (ui_wifi_sta_has_valid_ip()) {
            /* 再稳 400ms，避免刚拿到 IP 立刻开 WebServer */
            const unsigned long s0 = millis();
            bool ok = true;
            while ((millis() - s0) < 400u) {
                if (!ui_wifi_sta_has_valid_ip()) {
                    ok = false;
                    break;
                }
                delay(40);
            }
            if (ok) {
                W20_WIFI_LOG("[W20WifiOta] STA reconnected %s\n", WiFi.localIP().toString().c_str());
                return true;
            }
        }
        if ((millis() - lastLog) >= 3000) {
            lastLog = millis();
            W20_WIFI_LOG("[W20WifiOta] waiting STA status=%d ip=%s elapsed=%lus\n", (int)WiFi.status(),
                          WiFi.localIP().toString().c_str(), (unsigned long)((millis() - t0) / 1000));
        }
        delay(100);
    }

    setStatus("Need WiFi first");
    LOG_WARN("W20 WiFi OTA: STA not connected");
    return false;
}

bool w20_wifi_ota_prepare_admin_reboot(void)
{
    return w20_wifi_ota_prepare_sta();
}

bool w20_wifi_ota_start(void)
{
    if (s_running) {
        return true;
    }
    s_abortRequested = false;
    W20_WIFI_LOGLN("[W20WifiOta] start (STA only)...");
    ensureProvisioningStoppedForOta();
    if (ui_wifi_provisioning_active()) {
        setStatus("Close WIFI Config");
        Serial.println("[W20WifiOta] start blocked: provisioning AP still up");
        return false;
    }

    if (!w20_wifi_ota_prepare_sta()) {
        Serial.println("[W20WifiOta] start failed: need router WiFi");
        return false;
    }
    if (s_abortRequested) {
        setStatus("Cancelled");
        return false;
    }

    WiFi.softAPdisconnect(false);
    if (WiFi.getMode() != WIFI_STA) {
        WiFi.mode(WIFI_STA);
        delay(150);
    }
    /* softAPdisconnect 后确认 STA 仍在；否则拉回 */
    if (!ui_wifi_sta_has_valid_ip() && config.network.wifi_ssid[0]) {
        W20_WIFI_LOGLN("[W20WifiOta] STA missing after SoftAP off, reconnect");
        if (!w20_wifi_ota_prepare_sta()) {
            Serial.println("[W20WifiOta] start failed: need router WiFi");
            return false;
        }
    }

    /* 配网残留缓冲/空闲任务先清掉，腾堆；绝不因低堆拒绝启动 */
    ui_wifi_provisioning_reclaim_heap();
    factory_pause_flush_timer();
    w20_deckpro_epd_drain();
    {
        uint32_t heap = ESP.getFreeHeap();
        if (heap < W20_WIFI_OTA_MIN_HEAP) {
            W20_WIFI_LOG("[W20WifiOta] heap %u after reclaim — brief settle\n", (unsigned)heap);
            const unsigned long t0 = millis();
            while ((millis() - t0) < 800u) {
                if (s_abortRequested) {
                    setStatus("Cancelled");
                    return false;
                }
                delay(40);
            }
            heap = ESP.getFreeHeap();
        }
        W20_WIFI_LOG("[W20WifiOta] heap before HTTP %u\n", (unsigned)heap);
        if (heap < W20_WIFI_OTA_MIN_HEAP) {
            setStatus("Starting (low mem)");
            Serial.printf("[W20WifiOta] low heap %u — still start OTA (no reject)\n", (unsigned)heap);
        }
    }

    s_http = &s_httpSta;
    w20_wifi_apply_ps_coex_safe();
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    if (!beginOtaHttpServer()) {
        setStatus("HTTP start failed");
        Serial.println("[W20WifiOta] HTTP begin failed");
        s_http = nullptr;
        return false;
    }
    s_running = true;
    otaSetEpdFlushPaused(true);
    otaStartPrepareAsync();
    char url[64] = {0};
    w20_wifi_ota_get_url(url, sizeof(url));
    W20_WIFI_LOG("[W20WifiOta] listening %s mode=STA\n", url);
    setStatus("Server ready");
    return true;
}

void w20_wifi_ota_stop(void)
{
    s_abortRequested = true;
    otaSetEpdFlushPaused(false);
    s_prepRetryAfterMs = 0;
    s_prepCreateFailCount = 0;
    const unsigned long t0 = millis();
    while (s_prepareTask != nullptr && (millis() - t0) < 3000) {
        delay(10);
    }
    otaAbortPreparedSession();
    if (s_running && s_http) {
        s_http->stop();
        s_running = false;
        s_progress = -1;
        setStatus("Stopped");
        W20_WIFI_LOGLN("[W20WifiOta] Stopped");
    }
    s_http = nullptr;
    /* 停 OTA HTTP 后顺带清配网残留，腾堆给后续页面 */
    ui_wifi_provisioning_reclaim_heap();
}

void w20_wifi_ota_loop(void)
{
    /*
     * Meshtastic main loop used to honor rebootAtMsec after OTA.
     * MeshCore stubs only store the variable — must reboot here or the browser
     * shows 100% / "rebooting" while the device keeps running the old slot.
     */
    if (rebootAtMsec != 0 && (long)(millis() - rebootAtMsec) >= 0) {
        Serial.println("[W20WifiOta] OTA OK — esp_restart()");
        delay(200);
        esp_restart();
    }

    if (!s_running || !s_http) {
        return;
    }
    /* xTaskCreate / esp_ota_begin 曾失败：到期自动再试，避免只打一次日志后装聋 */
    if (!s_otaSessionReady && s_otaPrepareFailed && s_prepareTask == nullptr && !s_otaHandle &&
        s_prepRetryAfterMs != 0 && (long)(millis() - s_prepRetryAfterMs) >= 0 && s_prepCreateFailCount < 6) {
        W20_WIFI_LOGLN("[W20WifiOta] deferred OTA prep retry");
        otaStartPrepareAsync();
    }
    /* 空闲时多轮询几次，避免 EPD 长刷期间浏览器 GET 卡住 */
    const bool busy = s_progress >= 0 && s_progress < 100 && strcmp(s_status, "Success, rebooting") != 0;
    const int httpPasses = busy ? 96 : 48;
    for (int i = 0; i < httpPasses; i++) {
        s_http->handleClient();
        if ((i & 7) == 7) {
            yield();
        }
    }
}

bool w20_wifi_ota_is_running(void)
{
    return s_running;
}

bool w20_wifi_ota_is_busy(void)
{
    return s_progress >= 0 && s_progress < 100 && strcmp(s_status, "Success, rebooting") != 0;
}

bool w20_wifi_ota_is_ap_mode(void)
{
    return false;
}

void w20_wifi_ota_get_url(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }
    char ip[24] = {0};
    ui_wifi_get_ip(ip, sizeof(ip));
    if (ip[0] == '\0' || strcmp(ip, "0.0.0.0") == 0) {
        snprintf(buf, len, "Need WiFi first");
    } else {
        snprintf(buf, len, "http://%s:%d/ota", ip, W20_WIFI_OTA_PORT_STA);
    }
}

const char *w20_wifi_ota_get_status_text(void)
{
    return s_status;
}

int w20_wifi_ota_get_progress_pct(void)
{
    return s_progress;
}

} // extern "C"

#endif
