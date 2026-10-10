#include "w20_ip5356.h"

#if defined(W20_MESHNOLOGY)

#include "configuration.h"
#include "peripheral.h"
#include "utilities.h"
#include "w20_dbg_log.h"
#include "w20_power_battery.h"
#include <Wire.h>
#include <math.h>
#include <limits.h>

extern bool peri_init_st[E_PERI_NUM_MAX];

IP5356 g_ip5356;

static w20_ip5356_probe_state_t s_probe_state = W20_IP5356_PROBE_NOT_TRIED;
/** 上次真正执行 I2C begin 的时间（含 boot）；用于运行期限频。 */
static uint32_t s_last_begin_ms = 0;
/** 最近一次读寄存器是否成功（-1/false 记失败）。 */
static bool s_last_read_ok = false;
/** 由连续读失败进入的总线 offline（硬件掉电）；区别于开机从未找到。 */
static bool s_bus_offline = false;
static uint8_t s_fail_streak = 0;
static uint32_t s_last_recover_probe_ms = 0;

static void mark_bus_offline(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK)
        return;
    s_probe_state = W20_IP5356_PROBE_SKIPPED;
    s_bus_offline = true;
    s_fail_streak = 0;
    s_last_read_ok = false;
    s_last_recover_probe_ms = millis(); /* 立刻起算，满 RECOVER_MS 后再轻探 */
    W20_BAT_LOG("[IP5356] bus offline (I2C NACK), stop poll; light-probe every %ums\n",
                (unsigned)W20_IP5356_BUS_RECOVER_MS);
}

static void note_read_ok(bool ok)
{
    s_last_read_ok = ok;
    if (ok) {
        s_fail_streak = 0;
        return;
    }
    if (s_probe_state != W20_IP5356_PROBE_OK)
        return;
    if (s_fail_streak < 255)
        s_fail_streak++;
    if (s_fail_streak >= (uint8_t)W20_IP5356_BUS_FAIL_STREAK)
        mark_bus_offline();
}

static bool light_probe_ack(void)
{
    Wire.setClock(100000);
    Wire.beginTransmission(BOARD_I2C_ADDR_IP5356);
    const uint8_t err = Wire.endTransmission();
    Wire.setClock(400000);
    return err == 0;
}

/**
 * SKIPPED（开机未找到或运行掉电）后限频轻探；ACK 则 full begin。
 * 插电恢复主路径（约 BUS_RECOVER_MS），不必等 KEY 30s 周期。
 */
static void maybe_bus_recover(void)
{
    if (s_probe_state != W20_IP5356_PROBE_SKIPPED)
        return;

    const uint32_t now = millis();
    if (s_last_recover_probe_ms != 0 &&
        (uint32_t)(now - s_last_recover_probe_ms) < (uint32_t)W20_IP5356_BUS_RECOVER_MS)
        return;
    s_last_recover_probe_ms = now;

    if (!light_probe_ack())
        return;

    W20_BAT_LOG("[IP5356] bus ACK @ 0x%02X, reprobe\n", BOARD_I2C_ADDR_IP5356);
    s_bus_offline = false;
    s_fail_streak = 0;
    s_probe_state = W20_IP5356_PROBE_NOT_TRIED;
    if (w20_ip5356_boot_init()) {
        peri_init_st[E_PERI_IP5356] = true;
        w20_power_battery_on_ip5356_ready();
    } else {
        /* begin 仍失败：保持 SKIPPED，继续轻探 */
        s_probe_state = W20_IP5356_PROBE_SKIPPED;
    }
}

w20_ip5356_probe_state_t w20_ip5356_probe_state(void)
{
    return s_probe_state;
}

bool w20_ip5356_boot_init(void)
{
    if (s_probe_state == W20_IP5356_PROBE_OK)
        return g_ip5356.isPresent();

    /* NOT_TRIED 或由 reprobe 重置后的再探；SKIPPED 仅 boot 短路，reprobe 会先清状态 */
    if (s_probe_state == W20_IP5356_PROBE_SKIPPED)
        return false;

    Wire.setClock(100000);
    const bool ok = g_ip5356.begin(Wire, PIN_IP5356_INT, BOARD_I2C_ADDR_IP5356);
    Wire.setClock(400000);
    s_last_begin_ms = millis();

    if (ok) {
        s_probe_state = W20_IP5356_PROBE_OK;
        s_bus_offline = false;
        s_fail_streak = 0;
        const int mv = g_ip5356.readVoltageMv();
        const int p = g_ip5356.readPercent(IP5356_SOC_EMPTY_MV, IP5356_SOC_FULL_MV);
        note_read_ok(mv >= 0);
        LOG_INFO("IP5356 OK @ 0x%02X, VBAT=%d mV, SOC=%d%%", g_ip5356.address(), mv, p);
    } else {
        s_probe_state = W20_IP5356_PROBE_SKIPPED;
        s_bus_offline = false;
        s_last_recover_probe_ms = millis(); /* 与运行掉电相同：轻探恢复，插电尽快出充电态 */
        note_read_ok(false);
        s_fail_streak = 0;
        LOG_WARN("IP5356 not found @ 0x%02X, skip (light-probe / KEY may reprobe)", BOARD_I2C_ADDR_IP5356);
    }
    return ok;
}

bool w20_ip5356_reprobe(void)
{
    if (s_probe_state == W20_IP5356_PROBE_OK && g_ip5356.isPresent())
        return true;
    s_probe_state = W20_IP5356_PROBE_NOT_TRIED;
    s_bus_offline = false;
    s_fail_streak = 0;
    return w20_ip5356_boot_init();
}

bool w20_ip5356_try_runtime_reprobe(const char *reason)
{
    if (s_probe_state == W20_IP5356_PROBE_OK && g_ip5356.isPresent())
        return true;

    const uint32_t now = millis();
    if (s_last_begin_ms != 0 && (uint32_t)(now - s_last_begin_ms) < W20_IP5356_REPROBE_MIN_MS)
        return false;

    LOG_INFO("IP5356 runtime reprobe (%s)", reason ? reason : "?");
    return w20_ip5356_reprobe();
}

void w20_ip5356_ensure_ready(void)
{
    /* 故意为空：Battery/Test 读路径禁止 begin，避免反复 I2C 探测拖死总线。 */
}

bool w20_ip5356_is_present(void)
{
    maybe_bus_recover();
    return s_probe_state == W20_IP5356_PROBE_OK && g_ip5356.isPresent();
}

bool w20_ip5356_last_read_ok(void)
{
    return s_last_read_ok;
}

int w20_ip5356_read_voltage_mv(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int mv = g_ip5356.readVoltageMv();
    note_read_ok(mv >= 0);
    return mv;
}

int w20_ip5356_read_percent(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int p = g_ip5356.readPercent(IP5356_SOC_EMPTY_MV, IP5356_SOC_FULL_MV);
    note_read_ok(p >= 0);
    return p;
}

bool w20_ip5356_soc_from_register(int *percent)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return false;
    }
    const int p = g_ip5356.readSocPercent();
    if (p < 0) {
        note_read_ok(false);
        return false;
    }
    if (percent)
        *percent = p;
    note_read_ok(true);
    return true;
}

bool w20_ip5356_read_status(IP5356::Status &st)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return false;
    }
    const bool ok = g_ip5356.readStatus(st);
    note_read_ok(ok);
    return ok;
}

int w20_ip5356_read_input_current_ma(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int ma = g_ip5356.readInputCurrentMa();
    note_read_ok(ma >= 0);
    return ma;
}

int w20_ip5356_read_bat_current_ma(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int ma = g_ip5356.readBatCurrentMa();
    note_read_ok(ma >= 0);
    return ma;
}

int w20_ip5356_read_vin_mv(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int mv = g_ip5356.readVinMv();
    note_read_ok(mv >= 0);
    return mv;
}

int w20_ip5356_read_vbus_mv(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int mv = g_ip5356.readVbusMv();
    note_read_ok(mv >= 0);
    return mv;
}

int w20_ip5356_read_ntc_mv(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK) {
        s_last_read_ok = false;
        return -1;
    }
    const int mv = g_ip5356.readNtcMv();
    note_read_ok(mv >= 0);
    return mv;
}

int w20_ip5356_read_ntc_temp_c(void)
{
    if (s_probe_state != W20_IP5356_PROBE_OK)
        return INT16_MIN;
    const int mv = g_ip5356.readNtcMv();
    note_read_ok(mv >= 0);
    if (mv < 50 || mv > 3300)
        return INT16_MIN;
    const int iUa = g_ip5356.readNtcSourceCurrentUa(BOARD_I2C_ADDR_IP5356_CFG, IP5356_NTC_DEFAULT_I_UA);
    if (iUa <= 0)
        return INT16_MIN;
    const float rOhm = (float)mv * 1000.f / (float)iUa;
    if (rOhm < 100.f || rOhm > 500000.f)
        return INT16_MIN;
    const float lnRatio = logf(rOhm / (float)IP5356_NTC_R0_OHM);
    const float invT = (1.f / 298.15f) + (lnRatio / (float)IP5356_NTC_B_VALUE);
    if (invT <= 0.f)
        return INT16_MIN;
    const float tC = (1.f / invT) - 273.15f;
    if (tC < -40.f || tC > 125.f)
        return INT16_MIN;
    return (int)(tC + (tC >= 0.f ? 0.5f : -0.5f));
}

#endif /* W20_MESHNOLOGY */
