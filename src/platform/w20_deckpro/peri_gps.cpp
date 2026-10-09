
#include "utilities.h"
#include "peripheral.h"
#include "platform/w20_deckpro/w20_dbg_log.h"
#include <TinyGPS++.h>

/* clang-format off */

TinyGPSPlus deck_gps;
static bool GPS_Recovery();
bool setupGPS();
void displayInfo();

static TaskHandle_t gps_handle = nullptr;
static double gps_lat=0, gps_lng=0, gps_altitude=0, gps_speed=0, gps_course=0;
static uint16_t gps_year=0;
static uint8_t gps_month=0, gps_day=0;
static uint8_t gps_hour=0, gps_minute=0, gps_second=0;
static uint32_t gps_vsat=0;
static float gps_hdop=0.0f;
static bool gps_fix_valid=false;
static uint32_t s_deck_nmea_cnt = 0;
static uint32_t s_deck_gps_hb_ms = 0;

uint8_t buffer[256];

bool gps_init(void)
{
#if (BOARD_GPS_EN >= 0)
    if (digitalRead(BOARD_GPS_EN) != HIGH) {
        Serial.println(F("[GPS] init skipped: module power OFF (EN low)"));
        return false;
    }
#endif
    /*
     * Mesh 进页前已探测过 L76K。Deck 再跑 setupGPS()/PCAS 握手会阻塞数秒且常超时，
     * 表现为「刚进 GPS 页 init fail」。这里只重新挂 UART2 + 解码任务。
     */
    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RXD, BOARD_GPS_TXD);
    while (SerialGPS.available()) {
        SerialGPS.read();
    }
    gps_task_create();
    gps_task_resume();
    s_deck_nmea_cnt = 0;
    s_deck_gps_hb_ms = millis();
    W20_GPS_LOGLN("[GPS-Deck] gps_init OK: UART2 begin + task resume");
    return true;
}

void gps_task(void *param)
{
    (void)param;
    while (1) {
        while (Serial.available()) {
            SerialGPS.write(Serial.read());
        }

        while (SerialGPS.available()) {
            int c = SerialGPS.read();
            // Serial.write(c);
            if (deck_gps.encode(c)) {
                s_deck_nmea_cnt++;
                displayInfo();
            }
        }

        const uint32_t now = millis();
        if (now - s_deck_gps_hb_ms >= 3000) {
            s_deck_gps_hb_ms = now;
            const int en = (BOARD_GPS_EN >= 0) ? digitalRead(BOARD_GPS_EN) : -1;
            W20_GPS_LOG("[GPS-Deck] hb chars=%lu sentences=%lu fix=%d sats=%lu en=%d\r\n",
                        (unsigned long)deck_gps.charsProcessed(), (unsigned long)s_deck_nmea_cnt, gps_fix_valid ? 1 : 0,
                        (unsigned long)gps_vsat, en);
        }

        if (millis() > 30000 && deck_gps.charsProcessed() < 10) {
            W20_GPS_LOGLN("[GPS-Deck] No GPS detected: check wiring (no UART bytes)");
            delay(1000);
        }
        delay(1);
    }
}

void gps_task_create(void)
{
    if (gps_handle != nullptr) {
        return;
    }
    xTaskCreate(gps_task, "gps_task", 1024 * 3, NULL, GPS_PRIORITY, &gps_handle);
    /* 在创建 侧挂起，避免「resume 早于任务内自挂起」导致任务永久停住 */
    if (gps_handle != nullptr) {
        vTaskSuspend(gps_handle);
        W20_GPS_LOGLN("[GPS-Deck] task create (suspended)");
    }
}

void gps_task_suspend(void)
{
    if (gps_handle != NULL) {
        vTaskSuspend(gps_handle);
        W20_GPS_LOGLN("[GPS-Deck] task suspend");
    }
}

void gps_task_resume(void)
{
    if (gps_handle != NULL) {
        vTaskResume(gps_handle);
        W20_GPS_LOGLN("[GPS-Deck] task resume");
    }
}

void gps_get_coord(double *lat, double *lng)
{
    if (lat) {
        *lat = gps_lat;
    }
    if (lng) {
        *lng = gps_lng;
    }
}

void gps_get_data(uint16_t *year, uint8_t *month, uint8_t *day)
{
    if (year) {
        *year = gps_year;
    }
    if (month) {
        *month = gps_month;
    }
    if (day) {
        *day = gps_day;
    }
}

void gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (hour) {
        *hour = gps_hour;
    }
    if (minute) {
        *minute = gps_minute;
    }
    if (second) {
        *second = gps_second;
    }
}

void gps_get_satellites(uint32_t *vsat)
{
    if (vsat) {
        *vsat = gps_vsat; // Visible Satellites
    }
}

void gps_get_speed(double *speed)
{
    if (speed) {
        *speed = gps_speed;
    }
}

void gps_get_altitude(double *alt)
{
    if (alt) {
        *alt = gps_altitude;
    }
}

void gps_get_course(double *course)
{
    if (course) {
        *course = gps_course;
    }
}

void gps_get_hdop(float *hdop)
{
    if (hdop) {
        *hdop = gps_hdop;
    }
}

bool gps_get_fix_valid(void)
{
    return gps_fix_valid;
}

void gps_release_for_mesh(void)
{
    W20_GPS_LOGLN("[GPS-Deck] release_for_mesh (suspend+Serial2.end)");
    gps_task_suspend();
    delay(5);
    while (SerialGPS.available())
        SerialGPS.read();
    SerialGPS.end();
}

/** 从 Meshtastic 页回到九宫格：保持 GPS 任务挂起，关闭 UART（进入 GPS 子屏再由 ui_gps_task_resume 打开）。 */
void gps_prepare_idle_for_deck(void)
{
    gps_task_suspend();
    while (SerialGPS.available())
        SerialGPS.read();
    SerialGPS.end();
}

/** 进入 Deck GPS 子屏时：打开 UART 与 NMEA 任务（供电由 Setting Power GPS 控制，此处不再强制 EN/WAKE）。 */
void gps_enable_for_screen(void)
{
    W20_GPS_LOGLN("[GPS-Deck] enable_for_screen");
    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RXD, BOARD_GPS_TXD);
    gps_task_resume();
}

void gps_disable_for_screen(void)
{
    W20_GPS_LOGLN("[GPS-Deck] disable_for_screen");
    gps_task_suspend();
    while (SerialGPS.available())
        SerialGPS.read();
    SerialGPS.end();
}

/* clang-format on */
void displayInfo()
{
    Serial.print(F("Location: "));
    if (deck_gps.location.isValid())
    {
        gps_fix_valid = true;
        gps_lat = deck_gps.location.lat();
        gps_lng = deck_gps.location.lng();
        Serial.print(gps_lat, 6);
        Serial.print(F(","));
        Serial.print(gps_lng, 6);
    }
    else
    {
        gps_fix_valid = false;
        Serial.print(F("INVALID"));
    }

    Serial.print(F("  Date/Time: "));
    if (deck_gps.date.isValid())
    {
        gps_year = deck_gps.date.year();
        gps_month = deck_gps.date.month();
        gps_day = deck_gps.date.day();
        Serial.print(gps_month);
        Serial.print(F("/"));
        Serial.print(gps_day);
        Serial.print(F("/"));
        Serial.print(gps_year);
    }
    else
    {
        Serial.print(F("INVALID"));
    }

    Serial.print(F(" "));
    if (deck_gps.time.isValid())
    {
        gps_hour = deck_gps.time.hour();
        gps_minute = deck_gps.time.minute();
        gps_second = deck_gps.time.second();

        if (gps_hour < 10)
            Serial.print(F("0"));
        Serial.print(gps_hour);
        Serial.print(F(":"));
        if (gps_minute < 10)
            Serial.print(F("0"));
        Serial.print(gps_minute);
        Serial.print(F(":"));
        if (gps_second < 10)
            Serial.print(F("0"));
        Serial.print(gps_second);
        Serial.print(F("."));
    }
    else
    {
        Serial.print(F("INVALID"));
    }

    Serial.print(F("  Satellites: "));
    if(deck_gps.satellites.isValid())
    {
        gps_vsat = deck_gps.satellites.value();
        Serial.print(gps_vsat);
        Serial.print(F(" "));
    }

    Serial.print(F("  Speed: "));
    if(deck_gps.speed.isValid())
    {
        gps_speed = deck_gps.speed.kmph();
        Serial.print(gps_speed);
        Serial.print(F(" "));
    }

    if (deck_gps.altitude.isValid()) {
        gps_altitude = deck_gps.altitude.meters();
    }
    if (deck_gps.course.isValid()) {
        gps_course = deck_gps.course.deg();
    }
    if (deck_gps.hdop.isValid()) {
        gps_hdop = deck_gps.hdop.hdop();
    }

    Serial.println();
    W20_GPS_LOG("[GPS-Deck] NMEA parsed fix=%d sats=%lu lat=%.5f lon=%.5f\r\n", gps_fix_valid ? 1 : 0,
                (unsigned long)gps_vsat, gps_lat, gps_lng);
}
/* clang-format off */

bool setupGPS()
{
    // L76K GPS USE 9600 BAUDRATE
    SerialGPS.begin(9600, SERIAL_8N1, BOARD_GPS_RXD, BOARD_GPS_TXD);
    bool result = false;
    uint32_t startTimeout ;
    for (int i = 0; i < 3; ++i) {
        SerialGPS.write("$PCAS03,0,0,0,0,0,0,0,0,0,0,,,0,0*02\r\n");
        delay(5);
        // Get version information
        startTimeout = millis() + 3000;
        Serial.print("Try to init L76K . Wait stop .");
        while (SerialGPS.available()) {
            Serial.print(".");
            SerialGPS.readString();
            if (millis() > startTimeout) {
                Serial.println("Wait L76K stop NMEA timeout!");
                return false;
            }
        };
        Serial.println();
        SerialGPS.flush();
        delay(200);

        SerialGPS.write("$PCAS06,0*1B\r\n");
        startTimeout = millis() + 500;
        String ver = "";
        while (!SerialGPS.available()) {
            if (millis() > startTimeout) {
                Serial.println("Get L76K timeout!");
                return false;
            }
        }
        SerialGPS.setTimeout(10);
        ver = SerialGPS.readStringUntil('\n');
        if (ver.startsWith("$GPTXT,01,01,02")) {
            Serial.println("L76K GNSS init succeeded, using L76K GNSS Module\n");
            result = true;
            break;
        }
        delay(500);
    }
    // Initialize the L76K Chip, use GPS + GLONASS
    SerialGPS.write("$PCAS04,5*1C\r\n");
    delay(250);
    SerialGPS.write("$PCAS03,1,1,1,1,1,1,1,1,1,1,,,0,0*26\r\n");
    delay(250);
    // Switch to Vehicle Mode, since SoftRF enables Aviation < 2g
    SerialGPS.write("$PCAS11,3*1E\r\n");
    return result;
}


static int getAck(uint8_t *buffer, uint16_t size, uint8_t requestedClass, uint8_t requestedID)
{
    uint16_t    ubxFrameCounter = 0;
    bool        ubxFrame = 0;
    uint32_t    startTime = millis();
    uint16_t    needRead;

    while (millis() - startTime < 800) {
        while (SerialGPS.available()) {
            int c = SerialGPS.read();
            switch (ubxFrameCounter) {
            case 0:
                if (c == 0xB5) {
                    ubxFrameCounter++;
                }
                break;
            case 1:
                if (c == 0x62) {
                    ubxFrameCounter++;
                } else {
                    ubxFrameCounter = 0;
                }
                break;
            case 2:
                if (c == requestedClass) {
                    ubxFrameCounter++;
                } else {
                    ubxFrameCounter = 0;
                }
                break;
            case 3:
                if (c == requestedID) {
                    ubxFrameCounter++;
                } else {
                    ubxFrameCounter = 0;
                }
                break;
            case 4:
                needRead = c;
                ubxFrameCounter++;
                break;
            case 5:
                needRead |=  (c << 8);
                ubxFrameCounter++;
                break;
            case 6:
                if (needRead >= size) {
                    ubxFrameCounter = 0;
                    break;
                }
                if (SerialGPS.readBytes(buffer, needRead) != needRead) {
                    ubxFrameCounter = 0;
                } else {
                    return needRead;
                }
                break;

            default:
                break;
            }
        }
    }
    return 0;
}

static bool GPS_Recovery()
{
    uint8_t cfg_clear1[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1C, 0xA2};
    uint8_t cfg_clear2[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B, 0xA1};
    uint8_t cfg_clear3[] = {0xB5, 0x62, 0x06, 0x09, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x03, 0x1D, 0xB3};
    SerialGPS.write(cfg_clear1, sizeof(cfg_clear1));
    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successes!");
    }
    SerialGPS.write(cfg_clear2, sizeof(cfg_clear2));
    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successes!");
    }
    SerialGPS.write(cfg_clear3, sizeof(cfg_clear3));
    if (getAck(buffer, 256, 0x05, 0x01)) {
        Serial.println("Get ack successes!");
    }

    // UBX-CFG-RATE, Size 8, 'Navigation/measurement rate settings'
    uint8_t cfg_rate[] = {0xB5, 0x62, 0x06, 0x08, 0x00, 0x00, 0x0E, 0x30};
    SerialGPS.write(cfg_rate, sizeof(cfg_rate));
    if (getAck(buffer, 256, 0x06, 0x08)) {
        Serial.println("Get ack successes!");
    } else {
        return false;
    }
    return true;
}
