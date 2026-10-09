#pragma once

/**
 * 电压→SOC：仅 CW 失效时兜底。
 * 与 IP5356_SOC_EMPTY/FULL_MV（3.0–4.3V）线性一致：
 *   3000mV→0% … 4300mV→99%；UI 100% 只认拔电后 CW≥FULL_ENTER。
 * 实际计算见 w20_battery_lut_raw()（公式，不读下表）。
 * 下表仅作对照参考，勿再当作“CW 实测曲线”使用。
 */

#include <stdint.h>

typedef struct {
    int16_t mv;
    uint8_t pct;
} w20_bat_soc_lut_point_t;

#define W20_BAT_SOC_LUT_STEP_MV 50
#define W20_BAT_SOC_LUT_MIN_MV  3000
#define W20_BAT_SOC_LUT_MAX_MV  4300

/** 3.0–4.3V 线性对照（与 light/heavy 相同；带载 IR 压降由电流选表逻辑保留接口）。 */
static const w20_bat_soc_lut_point_t W20_BAT_SOC_LUT_LIGHT[] = {
    {3000, 0},  {3050, 3},  {3100, 7},  {3150, 11}, {3200, 15}, {3250, 19},
    {3300, 22}, {3350, 26}, {3400, 30}, {3450, 34}, {3500, 38}, {3550, 41},
    {3600, 45}, {3650, 49}, {3700, 53}, {3750, 57}, {3800, 60}, {3850, 64},
    {3900, 68}, {3950, 72}, {4000, 76}, {4050, 79}, {4100, 83}, {4150, 87},
    {4200, 91}, {4250, 95}, {4300, 99},
};

static const w20_bat_soc_lut_point_t W20_BAT_SOC_LUT_HEAVY[] = {
    {3000, 0},  {3050, 3},  {3100, 7},  {3150, 11}, {3200, 15}, {3250, 19},
    {3300, 22}, {3350, 26}, {3400, 30}, {3450, 34}, {3500, 38}, {3550, 41},
    {3600, 45}, {3650, 49}, {3700, 53}, {3750, 57}, {3800, 60}, {3850, 64},
    {3900, 68}, {3950, 72}, {4000, 76}, {4050, 79}, {4100, 83}, {4150, 87},
    {4200, 91}, {4250, 95}, {4300, 99},
};

#define W20_BAT_SOC_LUT_HEAVY_LEN (sizeof(W20_BAT_SOC_LUT_HEAVY) / sizeof(W20_BAT_SOC_LUT_HEAVY[0]))
#define W20_BAT_SOC_LUT_LIGHT_LEN (sizeof(W20_BAT_SOC_LUT_LIGHT) / sizeof(W20_BAT_SOC_LUT_LIGHT[0]))
