#include "w20_cw2217.h"

#if defined(W20_MESHNOLOGY)

#include "configuration.h"
#include "utilities.h"
#include <Wire.h>

#ifndef BOARD_I2C_ADDR_CW2217
#define BOARD_I2C_ADDR_CW2217 0x64
#endif

#define CW2217_REG_VERSION 0x00
#define CW2217_REG_VCELL 0x02
#define CW2217_REG_SOC 0x04
#define CW2217_REG_MODE_CONFIG 0x08
#define CW2217_REG_GPIO_CONFIG 0x0A
#define CW2217_REG_SOC_ALERT 0x0B
#define CW2217_REG_BAT_PROFILE 0x10
#define CW2217_REG_SOH 0xA6
#define CW2217_REG_IC_STATE 0xA7
#define CW2217_VERSION_ID 0xA0

#define CW2217_MODE_RESTART 0x30
#define CW2217_MODE_ACTIVE 0x00
#define CW2217_MODE_SLEEP 0xF0
#define CW2217_UPDATE_FLG 0x80
#define CW2217_IC_READY_MARK 0x0C

#define CW2217_PROFILE_SIZE 80
#define CW2217_READY_RETRY 50

/* Vendor-provided battery profile (80 bytes @ 0x10).
 * 与 2026-07 上位机 CSV「Read Battery Profile」一致。
 * 已知问题：FCC 工具显示 1000mAh、近满 SOC 系统性偏低（Finish 时 CW 常仅 ~67%）。
 * 勿手改本数组——须 Cellwise 按真实电芯重标定后整表替换（docs/CW2217_PROFILE_RECHAR.md）。
 * 固件侧用 W20_BAT_CW_SOC_FULL_RAW / FINISH_ICON_MIN_CW_PCT 做经验拉伸，不能替代新 Profile。
 */
static const uint8_t s_cw2217_profile[CW2217_PROFILE_SIZE] = {
    0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB1, 0xB0, 0xB4, 0xB3, 0xA1, 0x9A, 0xEA, 0xDF,
    0xD8, 0xFF, 0xF2, 0xBD, 0x9C, 0x76, 0x5D, 0x4D, 0x45, 0x45, 0x45, 0xAB, 0xA3, 0xDE, 0x60, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xCB, 0x38, 0x82, 0xB9, 0xD4, 0xE9, 0xF0, 0xCB, 0xA8, 0x92, 0x87,
    0x85, 0x8C, 0xA2, 0xBB, 0xCB, 0xFE, 0xFF, 0x12, 0x20, 0x00, 0xAB, 0x10, 0x00, 0xC0, 0x72, 0x00,
    0x00, 0x00, 0x64, 0xD9, 0xE3, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86,
};

enum {
    CW2217_STATE_OK = 0,
    CW2217_STATE_NOT_ACTIVE = 1,
    CW2217_STATE_PROFILE_NOT_READY = 2,
    CW2217_STATE_PROFILE_NEED_UPDATE = 3,
    CW2217_STATE_IO_ERR = -1,
};

static bool s_present = false;

static bool cw2217_write_u8(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(BOARD_I2C_ADDR_CW2217);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool cw2217_read(uint8_t reg, uint8_t *buf, size_t n)
{
    if (!buf || n == 0)
        return false;
    Wire.beginTransmission(BOARD_I2C_ADDR_CW2217);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;
    const size_t got = Wire.requestFrom((int)BOARD_I2C_ADDR_CW2217, (int)n);
    if (got != n)
        return false;
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t)Wire.read();
    return true;
}

static bool cw2217_sleep(void)
{
    if (!cw2217_write_u8(CW2217_REG_MODE_CONFIG, CW2217_MODE_RESTART))
        return false;
    delay(20);
    if (!cw2217_write_u8(CW2217_REG_MODE_CONFIG, CW2217_MODE_SLEEP))
        return false;
    delay(10);
    return true;
}

static bool cw2217_active(void)
{
    if (!cw2217_write_u8(CW2217_REG_MODE_CONFIG, CW2217_MODE_RESTART))
        return false;
    delay(20);
    if (!cw2217_write_u8(CW2217_REG_MODE_CONFIG, CW2217_MODE_ACTIVE))
        return false;
    delay(10);
    return true;
}

static bool cw2217_write_profile(void)
{
    for (int i = 0; i < CW2217_PROFILE_SIZE; i++) {
        if (!cw2217_write_u8((uint8_t)(CW2217_REG_BAT_PROFILE + i), s_cw2217_profile[i]))
            return false;
    }
    return true;
}

/** sleep → write profile → UPDATE_FLG → active → wait IC_READY */
static bool cw2217_config_start_ic(void)
{
    if (!cw2217_sleep())
        return false;
    if (!cw2217_write_profile())
        return false;
    if (!cw2217_write_u8(CW2217_REG_SOC_ALERT, CW2217_UPDATE_FLG))
        return false;
    if (!cw2217_write_u8(CW2217_REG_GPIO_CONFIG, 0x00))
        return false;
    if (!cw2217_active())
        return false;

    for (int i = 0; i < CW2217_READY_RETRY; i++) {
        delay(100);
        uint8_t st = 0;
        if (!cw2217_read(CW2217_REG_IC_STATE, &st, 1))
            continue;
        if ((st & CW2217_IC_READY_MARK) == CW2217_IC_READY_MARK)
            return true;
    }
    (void)cw2217_sleep();
    return false;
}

static int cw2217_get_state(void)
{
    uint8_t reg = 0;
    if (!cw2217_read(CW2217_REG_MODE_CONFIG, &reg, 1))
        return CW2217_STATE_IO_ERR;
    if (reg != CW2217_MODE_ACTIVE)
        return CW2217_STATE_NOT_ACTIVE;

    if (!cw2217_read(CW2217_REG_SOC_ALERT, &reg, 1))
        return CW2217_STATE_IO_ERR;
    if ((reg & CW2217_UPDATE_FLG) == 0)
        return CW2217_STATE_PROFILE_NOT_READY;

    for (int i = 0; i < CW2217_PROFILE_SIZE; i++) {
        if (!cw2217_read((uint8_t)(CW2217_REG_BAT_PROFILE + i), &reg, 1))
            return CW2217_STATE_IO_ERR;
        if (reg != s_cw2217_profile[i])
            return CW2217_STATE_PROFILE_NEED_UPDATE;
    }
    return CW2217_STATE_OK;
}

bool w20_cw2217_boot_init(void)
{
    Wire.setClock(100000);

    uint8_t ver = 0;
    if (!cw2217_read(CW2217_REG_VERSION, &ver, 1) || ver != CW2217_VERSION_ID) {
        Wire.setClock(400000);
        s_present = false;
        LOG_WARN("CW2217 not found @ 0x%02X (ver=0x%02X)", BOARD_I2C_ADDR_CW2217, ver);
        return false;
    }

    int st = cw2217_get_state();
    if (st < 0) {
        Wire.setClock(400000);
        s_present = false;
        LOG_WARN("CW2217 I2C error during state check");
        return false;
    }
    if (st != CW2217_STATE_OK) {
        LOG_INFO("CW2217 profile/config update (state=%d)", st);
        if (!cw2217_config_start_ic()) {
            Wire.setClock(400000);
            s_present = false;
            LOG_WARN("CW2217 profile load / IC ready timeout");
            return false;
        }
    }

    Wire.setClock(400000);
    s_present = true;

    /* First VCELL/SOC after active can be stale; brief settle. */
    delay(50);
    const int mv = w20_cw2217_read_voltage_mv();
    const int p = w20_cw2217_read_percent();
    const int soh = w20_cw2217_read_soh_percent();
    LOG_INFO("CW2217 OK @ 0x%02X, VBAT=%d mV, SOC=%d%%, SOH=%d%%", BOARD_I2C_ADDR_CW2217, mv, p, soh);
    return true;
}

bool w20_cw2217_is_present(void)
{
    return s_present;
}

bool w20_cw2217_try_recover(void)
{
    if (!s_present)
        return false;

    Wire.setClock(100000);
    const int st = cw2217_get_state();
    bool ok = false;
    if (st == CW2217_STATE_OK) {
        /* 已 ACTIVE：短延时后再读，有时刚唤醒寄存器仍为 0 */
        delay(30);
        ok = (w20_cw2217_read_voltage_mv() >= (int)W20_BAT_CW_VALID_MIN_MV);
    } else if (st == CW2217_STATE_NOT_ACTIVE) {
        ok = cw2217_active();
        if (ok) {
            for (int i = 0; i < 20; i++) {
                delay(50);
                uint8_t ic = 0;
                if (cw2217_read(CW2217_REG_IC_STATE, &ic, 1) &&
                    (ic & CW2217_IC_READY_MARK) == CW2217_IC_READY_MARK)
                    break;
            }
            ok = (w20_cw2217_read_voltage_mv() >= (int)W20_BAT_CW_VALID_MIN_MV);
        }
    } else if (st == CW2217_STATE_PROFILE_NOT_READY || st == CW2217_STATE_PROFILE_NEED_UPDATE) {
        ok = cw2217_config_start_ic();
    } else {
        /* IO 错：再探一次 begin 路径过重，仅试 active */
        ok = cw2217_active() && (w20_cw2217_read_voltage_mv() >= (int)W20_BAT_CW_VALID_MIN_MV);
    }
    Wire.setClock(400000);
    if (ok)
        LOG_INFO("CW2217 recover OK, VBAT=%d mV SOC=%d%%", w20_cw2217_read_voltage_mv(),
                 w20_cw2217_read_percent());
    else
        LOG_WARN("CW2217 recover failed (state=%d)", st);
    return ok;
}

int w20_cw2217_read_percent_x100(void)
{
    if (!s_present)
        return -1;
    uint8_t soc[2] = {0, 0};
    if (!cw2217_read(CW2217_REG_SOC, soc, 2))
        return -1;
    /* 0x04 整数 %；0x05 小数 = val/256 % */
    int p = (int)soc[0];
    if (p > 100)
        p = 100;
    const int frac = ((int)soc[1] * 100) / 256;
    return p * 100 + frac;
}

int w20_cw2217_read_percent(void)
{
    const int x100 = w20_cw2217_read_percent_x100();
    if (x100 < 0)
        return -1;
    int p = x100 / 100;
    if (p > 100)
        p = 100;
    return p;
}

int w20_cw2217_read_voltage_mv(void)
{
    if (!s_present)
        return -1;
    uint8_t v[2] = {0, 0};
    if (!cw2217_read(CW2217_REG_VCELL, v, 2))
        return -1;
    /* 14-bit, 312.5 µV/LSB → mV = raw * 5 / 16 */
    const uint16_t raw = (uint16_t)(((v[0] & 0x3F) << 8) | v[1]);
    return (int)((raw * 5u + 8u) / 16u);
}

int w20_cw2217_read_soh_percent(void)
{
    if (!s_present)
        return -1;
    uint8_t soh = 0;
    if (!cw2217_read(CW2217_REG_SOH, &soh, 1))
        return -1;
    if (soh > 100)
        return -1;
    return (int)soh;
}

#endif /* W20_MESHNOLOGY */
