#include "w20_power_battery.h"
#include "configuration.h"

#if defined(W20_MESHNOLOGY)

#include "HasBatteryLevel.h"
#include "power.h"
#include "utilities.h"
#include "w20_dbg_log.h"
#include "w20_ip5356.h"
#if USE_CW2217_GAUGE
#include "w20_cw2217.h"
#endif
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#include "rom/rtc.h"
#endif

/** KEY 持续低：IP5356 可能休眠；影响 LUT 选表，不强制改 UI%。 */
static bool s_key_sleep_mode = false;

enum {
    W20_BAT_SRC_NONE = 0,
    W20_BAT_SRC_CW2217 = 1,
    W20_BAT_SRC_V_LUT = 2,
    W20_BAT_SRC_IP5356 = 3,
};

static int s_ui_src = W20_BAT_SRC_NONE;
static int s_ui_display = -1;
static uint32_t s_ui_slew_ms = 0;
static bool s_ui_full_latched = false;
static int s_last_ibat_ma = 0;
static uint32_t s_last_ibat_ms = 0;
static bool s_chg_complete_latched = false;
static bool s_chg_complete_have = false;
static uint32_t s_chg_complete_last_true_ms = 0;
static bool s_vbus_was = false;
#if W20_BAT_SOC_ALIGN
/** 上次成功读到的插电态；IP 短暂 NACK 时保持 on_charge。 */
static bool s_vbus_latched = false;
static uint32_t s_vbus_ok_ms = 0;
#endif
/** 最近一次可信电池电压（过滤 IP KEY 毛刺 / CW 假 0）。 */
static int s_last_good_vbat_mv = -1;
static uint32_t s_cw_recover_ms = 0;
#if W20_BAT_UI_SRC_IP5356
/** IP 主源试跑：休眠/读失败时沿用的上次有效 IP%。 */
static int s_last_good_ip_pct = -1;
static uint32_t s_last_good_ip_ms = 0;
#endif
#if W20_BAT_SOC_ALIGN
/** IP−CW拉伸 的 EMA 偏置（百分点）；IP 休眠后仍保留。 */
static int s_align_bias = 0;
static uint32_t s_align_learn_ms = 0;
#if W20_BAT_SOC_ALIGN_VBAT_REF
/** 静载起点；0=当前非静载。 */
static uint32_t s_quiet_since_ms = 0;
/** 最近一次离开 heavy 的时刻；0=尚未经历 heavy。 */
static uint32_t s_heavy_end_ms = 0;
static bool s_was_heavy = false;
static uint32_t s_vbat_learn_ms = 0;
#if W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
/** 待静载确认后一次性 snap（开机 true；拔电边沿再置 true）。 */
static bool s_need_vbat_snap = true;
#endif
#endif
#endif
#if W20_BAT_SOC_CHG_RATCHET
/** 充电棘轮地板：插电期间 UI 不低于此值；估计超过后再抬。-1=未在充电棘轮。 */
static int s_chg_ui_floor = -1;
#endif

static int clamp_pct(int p)
{
    if (p < 0)
        return 0;
    if (p > 100)
        return 100;
    return p;
}

/**
 * CW 芯片% → UI%：过原点线性拉伸（真满 FULL_RAW → 100）。
 * 全程同一公式，避免「>5/10 才切换」在阈值处跳变；再经 slew 平滑。
 */
static int w20_battery_cw_scale_ui(int cw_chip)
{
    if (cw_chip < 0)
        return -1;
#if !W20_BAT_CW_SOC_SCALE
    return clamp_pct(cw_chip);
#else
    const int full = (int)W20_BAT_CW_SOC_FULL_RAW;
    if (full <= 0 || full >= 100)
        return clamp_pct(cw_chip);
    int ui = (cw_chip * 100 + full / 2) / full;
    return clamp_pct(ui);
#endif
}

static const char *bat_src_name(int src)
{
    switch (src) {
    case W20_BAT_SRC_IP5356:
        return "IP5356";
    case W20_BAT_SRC_CW2217:
        return "CW2217";
    case W20_BAT_SRC_V_LUT:
        return "V_LUT";
    default:
        return "none";
    }
}

const char *w20_battery_ui_src_name(void)
{
    return bat_src_name(s_ui_src);
}

void w20_battery_set_key_sleep_mode(bool on)
{
    s_key_sleep_mode = on;
}

bool w20_battery_key_sleep_mode(void)
{
    return s_key_sleep_mode;
}

#if W20_BAT_SOC_ALIGN
static uint32_t s_align_decay_ms = 0;
static int w20_battery_lut_raw(int mv, bool heavy); /* 静载 V 参考 / LUT 兜底共用 */

/** CW 见底或 VBAT 过低：清正 bias，避免 UI 卡在 bias_max。 */
static void w20_battery_align_attenuate(int cw_ui, int vbat_mv)
{
    if (s_align_bias <= 0)
        return;
    const bool cw_empty = (cw_ui >= 0 && cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT);
    const bool v_low = (vbat_mv > 0 && vbat_mv <= (int)W20_BAT_SOC_ALIGN_EMPTY_MV);
    if (cw_empty || v_low)
        s_align_bias = 0;
}

/**
 * bias 缓衰回 0：真离线（跟回 CW），或充电中消掉负 bias（避免拔电后突然掉）。
 * 插电锁存期间 charging=true → 正 bias 不衰（防 I2C NACK 把 UI 从近满打回去）。
 * 拔电后 VBAT≥HOLD（默认 3.6V）→ 正 bias 不衰（整段信 VBAT，勿抖一点就跟回 CW）。
 */
static void w20_battery_align_decay_bias(bool ip_online, bool charging, int vbat_mv)
{
    if (s_align_bias == 0)
        return;
    /* 充电（含 VBUS 锁存）：只衰负 bias；IP 在线放电：不衰 */
    if (charging) {
        if (s_align_bias >= 0)
            return;
    } else if (ip_online) {
        return;
    }
    /* ≥HOLD：正 bias 保着；静载 vbat-ref 可按 LUT 慢慢下调，勿直接衰到 CW */
    if (s_align_bias > 0 && vbat_mv >= (int)W20_BAT_SOC_ALIGN_HOLD_BIAS_MV)
        return;
    const uint32_t now = millis();
    if (s_align_decay_ms != 0 &&
        (uint32_t)(now - s_align_decay_ms) < (uint32_t)W20_BAT_SOC_ALIGN_DECAY_MS)
        return;
    s_align_decay_ms = now;
    if (s_align_bias > 0)
        s_align_bias--;
    else if (s_align_bias < 0)
        s_align_bias++;
}

static bool w20_battery_ip_finish_latched(void);

/**
 * 分阶段学 bias：|IP−CW| / 电压分歧大 → 更慢、步子更小，不一次钉死到 IP。
 * @param charging 充电：只学正方向（向 IP 攀升）；放电：原逻辑（heavy 停学）。
 */
static void w20_battery_align_learn(int ip_pct, int cw_ui, bool heavy, int cw_mv, int ip_mv, int vbat_mv,
                                    bool vbat_quiet, bool charging)
{
    if (ip_pct < 0 || cw_ui < 0 || s_key_sleep_mode)
        return;
    /* 放电：大电流 IR 勿学。充电：允许慢爬（下面强制大分歧档）。 */
    if (!charging && heavy)
        return;
    /* 见底时 err≈IP−0 会把 bias 顶满，禁止学习 */
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT)
        return;
    if (!w20_ip5356_is_present())
        return;

    /* 充电：未 Finish 时 IP 常虚报 100 → 目标封顶 CHARGE_CAP；Finish 后可学到 100 */
    int ip_tgt = ip_pct;
    if (charging) {
        if (!w20_battery_ip_finish_latched() && ip_tgt > (int)W20_BAT_SOC_CHARGE_CAP_PCT)
            ip_tgt = (int)W20_BAT_SOC_CHARGE_CAP_PCT;
        if (ip_tgt <= cw_ui)
            return;
    }

    const int err = ip_tgt - cw_ui;
    int gap = (err >= 0) ? err : -err;
    if (cw_mv >= (int)W20_BAT_CW_VALID_MIN_MV && ip_mv >= (int)W20_BAT_IP_VALID_MIN_MV) {
        int dv = ip_mv - cw_mv;
        if (dv < 0)
            dv = -dv;
        if (dv >= (int)W20_BAT_SOC_ALIGN_V_DISAGREE_MV && gap < (int)W20_BAT_SOC_ALIGN_GAP_MID)
            gap = (int)W20_BAT_SOC_ALIGN_GAP_MID;
    }
#if W20_BAT_SOC_ALIGN_VBAT_REF
    /* 静载下 IP% 与 OCV 尺差大：IP 不可猛信，至少按中/大分歧慢靠（放电路径） */
    if (!charging && vbat_quiet && vbat_mv >= (int)W20_BAT_CW_VALID_MIN_MV) {
        const int v_ref = w20_battery_lut_raw(vbat_mv, false);
        if (v_ref >= 0) {
            int dv_pct = ip_tgt - v_ref;
            if (dv_pct < 0)
                dv_pct = -dv_pct;
            if (dv_pct >= (int)W20_BAT_SOC_ALIGN_GAP_LARGE && gap < (int)W20_BAT_SOC_ALIGN_GAP_LARGE)
                gap = (int)W20_BAT_SOC_ALIGN_GAP_LARGE;
            else if (dv_pct >= (int)W20_BAT_SOC_ALIGN_GAP_MID && gap < (int)W20_BAT_SOC_ALIGN_GAP_MID)
                gap = (int)W20_BAT_SOC_ALIGN_GAP_MID;
        }
    }
#else
    (void)vbat_mv;
    (void)vbat_quiet;
#endif

    int div = (W20_BAT_SOC_ALIGN_EMA_DIV > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV : 8;
    int max_step = 2;
    uint32_t learn_ms = W20_BAT_SOC_ALIGN_LEARN_MS;
    if (charging) {
        /* 充电攀升：默认慢档；CV 尾（小电流+高 V）加快；大电流再更慢 */
        div = (W20_BAT_SOC_ALIGN_EMA_DIV_LARGE > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV_LARGE : 32;
        max_step = 1;
        learn_ms = W20_BAT_SOC_ALIGN_LEARN_MS_CHG;
        const int ib_mag = (s_last_ibat_ma < 0) ? -s_last_ibat_ma : s_last_ibat_ma;
        const bool ib_fresh =
            (s_last_ibat_ms != 0) && ((uint32_t)(millis() - s_last_ibat_ms) < 10000u);
        const int v_ref_mv =
            (vbat_mv >= (int)W20_BAT_CW_VALID_MIN_MV) ? vbat_mv
            : ((ip_mv >= (int)W20_BAT_IP_VALID_MIN_MV) ? ip_mv : 0);
        const bool cv_tail =
            ib_fresh && !heavy && (ib_mag <= (int)W20_BAT_SOC_ALIGN_CV_IBAT_MA) &&
            (v_ref_mv >= (int)W20_BAT_SOC_ALIGN_CV_MV);
        if (cv_tail) {
            learn_ms = W20_BAT_SOC_ALIGN_LEARN_MS_CHG_CV;
            max_step = 2;
            div = (W20_BAT_SOC_ALIGN_EMA_DIV_MID > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV_MID : 16;
        } else if (heavy || gap >= (int)W20_BAT_SOC_ALIGN_GAP_LARGE) {
            learn_ms = W20_BAT_SOC_ALIGN_LEARN_MS_LARGE;
        }
        if (!cv_tail && gap < (int)W20_BAT_SOC_ALIGN_GAP_LARGE)
            gap = (int)W20_BAT_SOC_ALIGN_GAP_LARGE;
    } else if (gap >= (int)W20_BAT_SOC_ALIGN_GAP_LARGE) {
        div = (W20_BAT_SOC_ALIGN_EMA_DIV_LARGE > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV_LARGE : 32;
        max_step = 1;
        learn_ms = W20_BAT_SOC_ALIGN_LEARN_MS_LARGE;
    } else if (gap >= (int)W20_BAT_SOC_ALIGN_GAP_MID) {
        div = (W20_BAT_SOC_ALIGN_EMA_DIV_MID > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV_MID : 16;
        max_step = 1;
    }

    const uint32_t now = millis();
    if (s_align_learn_ms != 0 && (uint32_t)(now - s_align_learn_ms) < learn_ms)
        return;
    s_align_learn_ms = now;

    int delta = (err - s_align_bias) / div;
    if (delta == 0 && err != s_align_bias)
        delta = (err > s_align_bias) ? 1 : -1;
    if (delta > max_step)
        delta = max_step;
    if (delta < -max_step)
        delta = -max_step;
    /* 充电只加正 bias，不学负向（防拔电掉 %） */
    if (charging && delta < 0)
        return;

    s_align_bias += delta;
    const int bias_max =
        charging ? (int)W20_BAT_SOC_ALIGN_BIAS_MAX_CHG : (int)W20_BAT_SOC_ALIGN_BIAS_MAX;
    if (s_align_bias > bias_max)
        s_align_bias = bias_max;
    if (s_align_bias < -(int)W20_BAT_SOC_ALIGN_BIAS_MAX)
        s_align_bias = -(int)W20_BAT_SOC_ALIGN_BIAS_MAX;
}

static int w20_battery_apply_align(int cw_ui, bool charging)
{
    if (cw_ui < 0)
        return -1;
    int bias = s_align_bias;
    /* 见底时正 bias 不参与，UI 可到 0（即使 attenuate 尚未跑） */
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT && bias > 0)
        bias = 0;
#if W20_BAT_SOC_CHG_RATCHET
    /* 充电：负 bias 不参与显示（等 CW 自己涨过再升） */
    if (charging && bias < 0)
        bias = 0;
#else
    (void)charging;
#endif
    return clamp_pct(cw_ui + bias);
}

#if W20_BAT_SOC_ALIGN_VBAT_REF
/**
 * 更新静载状态：无快充/快放、非 VBUS（插电抬压不可信）、大电流后已 settle。
 * IP 离线无 ibat 时（QUIET_NO_IBAT）仍可作候选——拔电后 NACK 常见。
 * @return 当前是否可采信 VBAT→LUT 作参考
 */
static bool w20_battery_vbat_quiet_update(bool heavy, bool have_ibat, int ibat_ma, bool vbus, int mv)
{
    const uint32_t now = millis();
    if (heavy) {
        s_was_heavy = true;
        s_quiet_since_ms = 0;
        return false;
    }
    if (s_was_heavy) {
        s_was_heavy = false;
        s_heavy_end_ms = now;
    }

    /* 插电时端电压被抬高，OCV 参考性差 */
    if (vbus || s_key_sleep_mode || mv < (int)W20_BAT_CW_VALID_MIN_MV) {
        s_quiet_since_ms = 0;
        return false;
    }
    if (have_ibat) {
        const int mag = (ibat_ma < 0) ? -ibat_ma : ibat_ma;
        if (mag >= (int)W20_BAT_SOC_ALIGN_QUIET_IBAT_MA) {
            s_quiet_since_ms = 0;
            return false;
        }
    } else if (!W20_BAT_SOC_ALIGN_QUIET_NO_IBAT) {
        /* 旧行为：无 ibat 永不 quiet */
        s_quiet_since_ms = 0;
        return false;
    }
    /* QUIET_NO_IBAT：IP 离线无电流可读，仍作静载候选（大电流由 heavy/last_ibat 挡住） */
    if (s_quiet_since_ms == 0)
        s_quiet_since_ms = now;

    if (s_heavy_end_ms != 0 &&
        (uint32_t)(now - s_heavy_end_ms) < (uint32_t)W20_BAT_SOC_ALIGN_HEAVY_SETTLE_MS)
        return false;
    return (uint32_t)(now - s_quiet_since_ms) >= (uint32_t)W20_BAT_SOC_ALIGN_QUIET_HOLD_MS;
}

/**
 * 近满静息滞回：进入 ≥REST_90，退出 ＜REST_90_EXIT，避免 4100↔4097 丢地板。
 * 插电清锁。
 */
static bool w20_battery_rest_near_hyst(int mv, bool on_charge)
{
    static bool s_rest_near = false;
    if (on_charge) {
        s_rest_near = false;
        return false;
    }
    if (mv >= (int)W20_BAT_NEAR_FULL_MV_REST_90)
        s_rest_near = true;
    else if (mv < (int)W20_BAT_NEAR_FULL_MV_REST_90_EXIT)
        s_rest_near = false;
    return s_rest_near;
}

/**
 * 静载电压软目标：≥HOLD(3.6V) 采信；线性 LUT，近满滞回再抬到地板（≥90/95）。
 * 避免 LUT≈83 仍被 bias_cap 钉在 76、或 4097 抖掉回 CW。
 */
static int w20_battery_vbat_soft_target(int mv, bool on_charge)
{
    if (mv < (int)W20_BAT_SOC_ALIGN_HOLD_BIAS_MV)
        return -1;
    int v_ref = w20_battery_lut_raw(mv, false);
    if (v_ref < 0)
        return -1;
    if (w20_battery_rest_near_hyst(mv, on_charge)) {
        const int floor = (mv >= (int)W20_BAT_NEAR_FULL_MV_99) ? 95 : 90;
        if (v_ref < floor)
            v_ref = floor;
    }
    return v_ref;
}

#if W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
/**
 * 开机/拔电后首次静载：按 VBAT 软目标一次性钉 bias，并跳过 UI slew。
 * 小 gap 也清 need 标志，避免反复触发。
 */
static void w20_battery_align_vbat_boot_snap(int cw_ui, int mv, bool quiet, bool on_charge)
{
    if (!s_need_vbat_snap || on_charge || !quiet || cw_ui < 0)
        return;
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT)
        return;
    if (mv < (int)W20_BAT_CW_VALID_MIN_MV)
        return;

    const int v_ref = w20_battery_vbat_soft_target(mv, on_charge);
    if (v_ref < 0)
        return;

    const int ui_now = clamp_pct(cw_ui + s_align_bias);
    int gap = ui_now - v_ref;
    if (gap < 0)
        gap = -gap;
    if (gap < (int)W20_BAT_SOC_ALIGN_VBAT_GAP_MIN) {
        s_need_vbat_snap = false;
        return;
    }

    int bias_max = (int)W20_BAT_SOC_ALIGN_BIAS_MAX;
    if (mv >= (int)W20_BAT_SOC_ALIGN_HOLD_BIAS_MV &&
        (int)W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX > bias_max)
        bias_max = (int)W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX;

    int target_bias = v_ref - cw_ui;
    if (target_bias > bias_max)
        target_bias = bias_max;
    if (target_bias < -(int)W20_BAT_SOC_ALIGN_BIAS_MAX)
        target_bias = -(int)W20_BAT_SOC_ALIGN_BIAS_MAX;

    const int old_bias = s_align_bias;
    s_align_bias = target_bias;
    s_need_vbat_snap = false;
    s_vbat_learn_ms = millis(); /* 本拍已对齐，勿立刻再慢纠一步 */

    const int ui_new = clamp_pct(cw_ui + s_align_bias);
    if (s_ui_display >= 0)
        s_ui_display = ui_new;

    W20_BAT_LOG("[BAT] vbat-snap: V=%dmV→%d%% cw=%d%% bias=%d→%d ui→%d%% (gap=%d)\n", mv, v_ref, cw_ui,
                old_bias, s_align_bias, ui_new, gap);
}
#endif /* W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP */

/**
 * 静载 VBAT→LUT 作软目标：|UI−Vref| 大才慢纠 bias，不一次钉死。
 * IP 在线时与 IP 学并行：电压只作「别偏太离谱」的阶段性拉回。
 * ≥HOLD(3.6V) 放宽 bias 上限；近满滞回地板避免 4097 抖掉。
 */
static void w20_battery_align_vbat_ref(int cw_ui, int mv, bool quiet, bool on_charge)
{
    if (!quiet || cw_ui < 0)
        return;
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT)
        return;
#if W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
    /* 仍待 snap：留给 boot_snap，避免先慢爬再跳 */
    if (s_need_vbat_snap)
        return;
#endif

    const int v_ref = w20_battery_vbat_soft_target(mv, on_charge);
    if (v_ref < 0)
        return;

    const int ui = clamp_pct(cw_ui + s_align_bias);
    int gap = ui - v_ref;
    if (gap < 0)
        gap = -gap;
    if (gap < (int)W20_BAT_SOC_ALIGN_VBAT_GAP_MIN)
        return;

    const uint32_t now = millis();
    if (s_vbat_learn_ms != 0 &&
        (uint32_t)(now - s_vbat_learn_ms) < (uint32_t)W20_BAT_SOC_ALIGN_VBAT_LEARN_MS)
        return;
    s_vbat_learn_ms = now;

    const int target_bias = v_ref - cw_ui;
    const int div = (W20_BAT_SOC_ALIGN_VBAT_EMA_DIV > 0) ? (int)W20_BAT_SOC_ALIGN_VBAT_EMA_DIV : 16;
    int max_step = (int)W20_BAT_SOC_ALIGN_VBAT_MAX_STEP;
    if (max_step < 1)
        max_step = 1;
    int delta = (target_bias - s_align_bias) / div;
    if (delta == 0 && target_bias != s_align_bias)
        delta = (target_bias > s_align_bias) ? 1 : -1;
    if (delta > max_step)
        delta = max_step;
    if (delta < -max_step)
        delta = -max_step;
    s_align_bias += delta;

    int bias_max = (int)W20_BAT_SOC_ALIGN_BIAS_MAX;
    if (mv >= (int)W20_BAT_SOC_ALIGN_HOLD_BIAS_MV &&
        (int)W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX > bias_max)
        bias_max = (int)W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX;
    if (s_align_bias > bias_max)
        s_align_bias = bias_max;
    if (s_align_bias < -(int)W20_BAT_SOC_ALIGN_BIAS_MAX)
        s_align_bias = -(int)W20_BAT_SOC_ALIGN_BIAS_MAX;

    static uint32_t s_vbat_ref_log_ms = 0;
    if (s_vbat_ref_log_ms == 0 || (uint32_t)(now - s_vbat_ref_log_ms) >= 10000u) {
        s_vbat_ref_log_ms = now;
        W20_BAT_LOG("[BAT] vbat-ref quiet: V=%dmV→%d%% ui=%d%% bias=%d→%d (gap=%d)\n", mv, v_ref, ui,
                    s_align_bias - delta, s_align_bias, gap);
    }
}
#endif /* W20_BAT_SOC_ALIGN_VBAT_REF */
#endif

int w20_battery_soc_align_bias(void)
{
#if W20_BAT_SOC_ALIGN
    return s_align_bias;
#else
    return 0;
#endif
}

static bool w20_battery_want_heavy_lut(int ibat_ma, bool have_ibat)
{
    const int mag = (ibat_ma < 0) ? -ibat_ma : ibat_ma;
    if (have_ibat)
        return mag >= (int)W20_BAT_SOC_HEAVY_IBAT_MA;
    if (s_last_ibat_ms != 0 && (uint32_t)(millis() - s_last_ibat_ms) < 60000u) {
        const int last = (s_last_ibat_ma < 0) ? -s_last_ibat_ma : s_last_ibat_ma;
        return last >= (int)W20_BAT_SOC_HEAVY_IBAT_MA;
    }
    return s_key_sleep_mode;
}

int w20_battery_read_vbat_mv(void)
{
    int cand = -1;

#if USE_CW2217_GAUGE
    /* 仅采纳有效 VCELL；假 0 / <2.0V 不得污染 LUT（否则会强制 UI=0%） */
    if (w20_cw2217_is_present()) {
        const int mv = w20_cw2217_read_voltage_mv();
        if (mv >= (int)W20_BAT_CW_VALID_MIN_MV)
            cand = mv;
    }
#endif

    if (cand < 0 && w20_ip5356_is_present()) {
        const int mv = w20_ip5356_read_voltage_mv();
        /* KEY 低时 IP 常见 ~0.6V 毛刺：一律丢弃，沿用上次有效值 */
        if (mv >= (int)W20_BAT_IP_VALID_MIN_MV) {
            /* 健康电压突然暴跌 >1.2V 也视为毛刺（一轮 KEY 脉冲） */
            if (s_last_good_vbat_mv >= 3500 && mv + 1200 < s_last_good_vbat_mv &&
                (s_key_sleep_mode || mv < 3200)) {
                W20_BAT_LOG("[BAT] IP vbat glitch %dmV (last=%dmV key_sleep=%d) ignored\n", mv,
                            s_last_good_vbat_mv, s_key_sleep_mode ? 1 : 0);
            } else {
                cand = mv;
            }
        } else if (mv > 0) {
            static uint32_t s_ip_glitch_log_ms = 0;
            const uint32_t now = millis();
            if (s_ip_glitch_log_ms == 0 || (uint32_t)(now - s_ip_glitch_log_ms) >= 5000u) {
                s_ip_glitch_log_ms = now;
                W20_BAT_LOG("[BAT] IP vbat %dmV < %dmV ignored (KEY/sleep glitch)\n", mv,
                            (int)W20_BAT_IP_VALID_MIN_MV);
            }
        }
    }

    if (cand > 0) {
        s_last_good_vbat_mv = cand;
        return cand;
    }
    return s_last_good_vbat_mv;
}

static int w20_battery_slew_toward(int target)
{
    const uint32_t now = millis();
    if (target < 0)
        return s_ui_display;
    if (s_ui_display < 0) {
        s_ui_display = clamp_pct(target);
        s_ui_slew_ms = now;
        return s_ui_display;
    }
    if (s_ui_display == clamp_pct(target))
        return s_ui_display;

    /* 每 (1000/SLEW) ms 最多追赶 1% */
    const uint32_t interval_ms =
        (W20_BAT_SOC_SLEW_PCT_PER_S > 0) ? (1000u / (uint32_t)W20_BAT_SOC_SLEW_PCT_PER_S) : 1000u;
    if (s_ui_slew_ms != 0 && (uint32_t)(now - s_ui_slew_ms) < interval_ms)
        return s_ui_display;

    if (s_ui_display < target)
        s_ui_display++;
    else
        s_ui_display--;
    s_ui_slew_ms = now;
    return s_ui_display;
}

/**
 * 近满 / 充满对齐：
 * - 对号亮 → UI 100（插电亦可）
 * - 插电且未充满 → 封顶 99
 * - 拔电 → raw_pct（IP 或 CW UI）达到 FULL_ENTER 才锁 100
 */
static int w20_battery_apply_full_hysteresis(int slewed_pct, int raw_pct, bool charge_cap_99)
{
    if (slewed_pct < 0)
        return slewed_pct;

    /* 期望：100 + 对号 + 满格；对号条件满足时不再卡在 99 */
    if (w20_battery_charge_complete_latched()) {
        s_ui_full_latched = true;
        s_ui_display = 100;
        return 100;
    }

    if (charge_cap_99) {
        s_ui_full_latched = false;
        if (slewed_pct > (int)W20_BAT_SOC_CHARGE_CAP_PCT)
            slewed_pct = (int)W20_BAT_SOC_CHARGE_CAP_PCT;
        if (s_ui_display > (int)W20_BAT_SOC_CHARGE_CAP_PCT)
            s_ui_display = (int)W20_BAT_SOC_CHARGE_CAP_PCT;
        return slewed_pct;
    }

    /* 拔电：以主源 raw 进入/退出 100（IP 试跑用 IP%；CW 模式用映射后 CW） */
    if (!s_ui_full_latched) {
        if (raw_pct >= (int)W20_BAT_SOC_FULL_ENTER)
            s_ui_full_latched = true;
    } else if (raw_pct >= 0 && raw_pct <= (int)W20_BAT_SOC_FULL_EXIT) {
        s_ui_full_latched = false;
        s_chg_complete_latched = false;
        s_chg_complete_have = false;
    } else if (slewed_pct <= (int)W20_BAT_SOC_FULL_EXIT) {
        s_ui_full_latched = false;
    }
    if (s_ui_full_latched) {
        s_ui_display = 100;
        return 100;
    }
    return slewed_pct;
}

/** IP 侧 Finish 锁存（不含 CW 门槛）；仅内部用。 */
static bool w20_battery_ip_finish_latched(void)
{
    const uint32_t now = millis();
    if (w20_ip5356_is_present()) {
        IP5356::Status st;
        if (w20_ip5356_read_status(st)) {
            const bool fin = (st.chgState == IP5356::kChgStateFull);
            s_chg_complete_latched = fin;
            s_chg_complete_have = true;
            if (fin)
                s_chg_complete_last_true_ms = now;
            /* 仍插电但已非 Finish：立刻清锁 */
            if (st.inputPresent && !fin) {
                s_chg_complete_latched = false;
            }
            return s_chg_complete_latched;
        }
    }
    /* IP 离线：短时保持 Finish，超时后放弃 */
    if (s_chg_complete_have && s_chg_complete_latched) {
        if ((uint32_t)(now - s_chg_complete_last_true_ms) <= (uint32_t)W20_BAT_FINISH_LATCH_HOLD_MS)
            return true;
        s_chg_complete_latched = false;
        s_chg_complete_have = false;
    }
    return false;
}

bool w20_battery_charge_complete_latched(void)
{
    if (!w20_battery_ip_finish_latched())
        return false;
#if W20_BAT_UI_SRC_IP5356
    /* IP 主源试跑：对号只认 IP Finish */
    return true;
#else
#if USE_CW2217_GAUGE
    /* CW 主源：Finish +（拉伸±IP bias 后）近满 */
    if (!w20_cw2217_is_present())
        return false;
    int cwUi = w20_battery_cw_scale_ui(w20_cw2217_read_percent());
#if W20_BAT_SOC_ALIGN
    /* 对号门槛：用当前 bias；充电中忽略负 bias，与 UI 一致 */
    bool fin_chg = false;
    if (w20_ip5356_is_present()) {
        IP5356::Status st_fin;
        if (w20_ip5356_read_status(st_fin))
            fin_chg = st_fin.inputPresent || st_fin.charging;
    }
    cwUi = w20_battery_apply_align(cwUi, fin_chg);
#endif
    if (cwUi < (int)W20_BAT_FINISH_ICON_MIN_CW_PCT)
        return false;
#endif
    return true;
#endif
}

/** 插电且未充满 → 充电过程中 UI% 封顶 99；对号亮时不封顶。 */
static bool w20_battery_charge_cap_99_active(void)
{
    if (!w20_ip5356_is_present())
        return false;
    IP5356::Status st;
    if (!w20_ip5356_read_status(st))
        return false;
    if (!st.inputPresent)
        return false;
    /* 对号已亮：允许 100，由 apply_full_hysteresis 写出 */
    if (w20_battery_charge_complete_latched())
        return false;
    return true;
}

#if W20_LOG_BAT
static int s_ui_src_logged = -1;
static int s_ui_pct_logged = -1;
static uint32_t s_ui_pct_log_ms = 0;

static void w20_battery_soc_src_log_if_changed(int src, int pct, int ipPct, int cwRaw, int ipPresent, int ipReadOk,
                                               int cwPresent)
{
    if (src == s_ui_src_logged)
        return;
    const char *from = (s_ui_src_logged < 0) ? "init" : bat_src_name(s_ui_src_logged);
    W20_BAT_LOG("[BAT] SOC src %s -> %s ui=%d%% ip=%d%% cw=%d%% bias=%+d key_sleep=%d "
                "ip_present=%d ip_read_ok=%d cw_present=%d t=%lu\n",
                from, bat_src_name(src), pct, ipPct, cwRaw, w20_battery_soc_align_bias(),
                s_key_sleep_mode ? 1 : 0, ipPresent, ipReadOk, cwPresent, (unsigned long)millis());
    s_ui_src_logged = src;
    if (pct >= 0) {
        s_ui_pct_logged = pct;
        s_ui_pct_log_ms = millis();
    }
}

static void w20_battery_pct_jump_log_if_needed(int src, int pct, int ipPct, int cwRaw)
{
    if (pct < 0)
        return;
    const uint32_t now = millis();
    if (s_ui_pct_logged < 0) {
        s_ui_pct_logged = pct;
        s_ui_pct_log_ms = now;
        return;
    }
    const int d = pct - s_ui_pct_logged;
    if (d >= W20_BAT_PCT_JUMP_LOG || d <= -W20_BAT_PCT_JUMP_LOG) {
        W20_BAT_LOG("[BAT] pct jump %d -> %d (d=%+d) src=%s ip=%d%% cw=%d%% bias=%+d key_sleep=%d dt=%lums t=%lu\n",
                    s_ui_pct_logged, pct, d, bat_src_name(src), ipPct, cwRaw, w20_battery_soc_align_bias(),
                    s_key_sleep_mode ? 1 : 0, (unsigned long)(now - s_ui_pct_log_ms), (unsigned long)now);
        s_ui_pct_logged = pct;
        s_ui_pct_log_ms = now;
    }
}
#endif

static int w20_battery_lut_raw(int mv, bool heavy)
{
    (void)heavy; /* 与 IP 线性尺同一把；接口保留便于日后再分带载表 */
    if (mv <= 0)
        return -1;
    if (mv <= (int)IP5356_SOC_EMPTY_MV)
        return 0;
    if (mv >= (int)IP5356_SOC_FULL_MV)
        return (int)W20_BAT_SOC_VOLTAGE_CAP_PCT; /* LUT 封顶 99；100 只认 CW≥FULL_ENTER */
    const int span = (int)IP5356_SOC_FULL_MV - (int)IP5356_SOC_EMPTY_MV;
    if (span <= 0)
        return -1;
    return ((mv - (int)IP5356_SOC_EMPTY_MV) * (int)W20_BAT_SOC_VOLTAGE_CAP_PCT) / span;
}

/**
 * 近满电压地板：仅小幅抬升，且 CW 已近满。
 * 禁止「充到 78% 拔电 → V 回弹过阈值 → 地板 99 → 锁 100」。
 * 拔电静息：门槛更低；≥4.10V 进地板≥90（退出＜4.00V 滞回），≥4.18→95；封顶 95。
 */
static int w20_battery_near_full_floor_pct(int mv, int cw_pct, int ip_pct, bool heavy_load, int ibat_ma,
                                          bool have_ibat, bool on_charge)
{
#if !W20_BAT_NEAR_FULL_FLOOR
    (void)mv;
    (void)cw_pct;
    (void)ip_pct;
    (void)heavy_load;
    (void)ibat_ma;
    (void)have_ibat;
    (void)on_charge;
    return -1;
#else
    (void)ip_pct; /* 不再用 IP% 抬地板 */
    if (heavy_load)
        return -1;

#if W20_BAT_SOC_ALIGN && W20_BAT_SOC_ALIGN_VBAT_REF
    const bool rest_near = w20_battery_rest_near_hyst(mv, on_charge);
#else
    const bool rest_near =
        !on_charge && mv >= (int)W20_BAT_NEAR_FULL_MV_REST_90;
#endif
    /* 普通近满仍要 ≥4100；静息滞回锁存后可低至 EXIT(4000)，避免 4097 丢地板 */
    if (!rest_near && mv < (int)W20_BAT_NEAR_FULL_MV_90)
        return -1;
    if (rest_near && mv < (int)W20_BAT_NEAR_FULL_MV_REST_90_EXIT)
        return -1;
    const int min_cw = on_charge ? (int)W20_BAT_NEAR_FULL_MIN_CW_PCT_CHG
                                 : (rest_near ? (int)W20_BAT_NEAR_FULL_MIN_CW_PCT_REST
                                              : (int)W20_BAT_NEAR_FULL_MIN_CW_PCT);
    if (cw_pct < 0 || cw_pct < min_cw)
        return -1;
    if (have_ibat) {
        const int mag = (ibat_ma < 0) ? -ibat_ma : ibat_ma;
        if (mag > (int)W20_BAT_NEAR_FULL_MAX_IBAT_MA)
            return -1;
    }

    int floor = 90;
    int max_lift = (int)W20_BAT_NEAR_FULL_MAX_LIFT_PCT;
    if (rest_near) {
        /* 满电≈4.2V：滞回保 ≥90；≥4.18→95。勿 4097 抖回 CW。 */
        floor = (mv >= (int)W20_BAT_NEAR_FULL_MV_99) ? 95 : 90;
        max_lift = (int)W20_BAT_NEAR_FULL_MAX_LIFT_PCT_REST;
    } else if (mv >= (int)W20_BAT_NEAR_FULL_MV_99) {
        floor = (int)W20_BAT_SOC_CHARGE_CAP_PCT; /* 地板最多 99；对号亮时由 hysteresis 升到 100 */
    } else if (mv >= (int)W20_BAT_NEAR_FULL_MV_95) {
        floor = 95;
    }

    if ((floor - cw_pct) < (int)W20_BAT_NEAR_FULL_LAG_PCT)
        return -1;
    /* 相对 CW 限幅抬升 */
    const int capped = cw_pct + max_lift;
    if (floor > capped)
        floor = capped;
    if (floor <= cw_pct)
        return -1;
    return floor;
#endif
}

int w20_battery_read_percent(void)
{
    const int ipPresent = w20_ip5356_is_present() ? 1 : 0;
    int ipPct = -1;
    int cwRaw = -1;
    int src = W20_BAT_SRC_NONE;
    int raw_pct = -1;
#if USE_CW2217_GAUGE
    const int cwPresent = w20_cw2217_is_present() ? 1 : 0;
#else
    const int cwPresent = 0;
#endif

#if USE_CW2217_GAUGE
    int cwMv = -1;
    if (cwPresent) {
        cwMv = w20_cw2217_read_voltage_mv();
        cwRaw = w20_cw2217_read_percent();
        /* present 但 VCELL≈0：芯片未测到电池，勿把 SOC=0 当有效主源 */
        if (cwRaw >= 0 && cwMv < (int)W20_BAT_CW_VALID_MIN_MV) {
            static uint32_t s_cw_bad_log_ms = 0;
            const uint32_t now_bad = millis();
            if (s_cw_bad_log_ms == 0 || (uint32_t)(now_bad - s_cw_bad_log_ms) >= 5000u) {
                s_cw_bad_log_ms = now_bad;
                W20_BAT_LOG("[BAT] CW invalid: soc=%d%% vcell=%dmV (< %dmV) → LUT fallback\n", cwRaw, cwMv,
                            (int)W20_BAT_CW_VALID_MIN_MV);
            }
            cwRaw = -1;
            /* 限频 wake：开机后变 0 多为 MODE 掉出 ACTIVE */
            if (s_cw_recover_ms == 0 || (uint32_t)(now_bad - s_cw_recover_ms) >= W20_BAT_CW_RECOVER_MS) {
                s_cw_recover_ms = now_bad;
                if (w20_cw2217_try_recover()) {
                    cwMv = w20_cw2217_read_voltage_mv();
                    cwRaw = w20_cw2217_read_percent();
                    if (cwRaw < 0 || cwMv < (int)W20_BAT_CW_VALID_MIN_MV)
                        cwRaw = -1;
                }
            }
        }
    }
#endif

    /* 仅用于 LUT 兜底选表 / 近满地板 / 低电关机电流判断；不参与 UI% 主路径 */
    int ibat = 0;
    bool have_ibat = false;
    int ipMv = -1;
    if (ipPresent) {
        ipPct = w20_ip5356_read_percent();
        ipMv = w20_ip5356_read_voltage_mv();
        ibat = w20_ip5356_read_bat_current_ma();
        have_ibat = true;
        s_last_ibat_ma = ibat;
        s_last_ibat_ms = millis();
    }

    const bool heavy = w20_battery_want_heavy_lut(ibat, have_ibat);
    const int mv = w20_battery_read_vbat_mv();

    bool vbus = false;
    bool charging = false;
    bool st_ok = false;
    if (ipPresent) {
        IP5356::Status st;
        if (w20_ip5356_read_status(st)) {
            st_ok = true;
            vbus = st.inputPresent;
            charging = st.charging;
        }
    }
#if W20_BAT_SOC_ALIGN
    /* 插电锁存：成功读到则更新；失败则在 HOLD 内保持，避免 NACK 当拔电衰 bias */
    if (st_ok) {
        s_vbus_latched = (vbus || charging);
        s_vbus_ok_ms = millis();
    } else if (s_vbus_latched) {
        if ((uint32_t)(millis() - s_vbus_ok_ms) > (uint32_t)W20_BAT_SOC_ALIGN_VBUS_HOLD_MS)
            s_vbus_latched = false;
    }
    const bool on_charge = (vbus || charging) || s_vbus_latched;
#else
    const bool on_charge = vbus || charging;
#endif
#if USE_CW2217_GAUGE
    int cwUi = w20_battery_cw_scale_ui(cwRaw);
#if W20_BAT_SOC_ALIGN && !W20_BAT_UI_SRC_IP5356
    /* 分阶段学 bias；充电正攀升+衰负 bias；静载 VBAT（仅非充）；见底/低 V 清正 bias */
#if W20_BAT_SOC_ALIGN_VBAT_REF
    const bool vbat_quiet = w20_battery_vbat_quiet_update(heavy, have_ibat, ibat, on_charge, mv);
#else
    const bool vbat_quiet = false;
#endif
    if (ipPct >= 0)
        w20_battery_align_learn(ipPct, cwUi, heavy, cwMv, ipMv, mv, vbat_quiet, on_charge);
    w20_battery_align_decay_bias(ipPct >= 0, on_charge, mv);
#if W20_BAT_SOC_ALIGN_VBAT_REF
#if W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
    w20_battery_align_vbat_boot_snap(cwUi, mv, vbat_quiet, on_charge);
#endif
    w20_battery_align_vbat_ref(cwUi, mv, vbat_quiet, on_charge);
#endif
    w20_battery_align_attenuate(cwUi, mv);
    cwUi = w20_battery_apply_align(cwUi, on_charge);
#endif
#else
    const int cwUi = -1;
#endif

    /* 拔电边沿（含锁存到期）：主源未到近满则清满电锁，禁止假 100% */
    if (s_vbus_was && !on_charge) {
#if W20_BAT_UI_SRC_IP5356
        const int edgePct = ipPct;
#else
        const int edgePct = cwUi;
#endif
        if (edgePct < (int)W20_BAT_SOC_FULL_ENTER) {
            s_chg_complete_latched = false;
            s_chg_complete_have = false;
            s_ui_full_latched = false;
        }
#if W20_BAT_SOC_CHG_RATCHET
        s_chg_ui_floor = -1;
#endif
#if W20_BAT_SOC_ALIGN_VBAT_REF && W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
        /* 拔电后再确认一次静载 VBAT，直接对齐，勿慢爬 */
        s_need_vbat_snap = true;
#endif
    }
    s_vbus_was = on_charge;

    /*
     * UI%：默认 CW（拉伸 + IP bias）→ LUT；可选 IP 主源试跑（宏=1）。
     */
#if W20_BAT_UI_SRC_IP5356
    {
        const uint32_t now_ip = millis();
        const bool ip_ui_ok = (ipPct >= 0) && !s_key_sleep_mode;
        if (ip_ui_ok) {
            s_last_good_ip_pct = clamp_pct(ipPct);
            s_last_good_ip_ms = now_ip;
            src = W20_BAT_SRC_IP5356;
            raw_pct = s_last_good_ip_pct;
        } else if (s_last_good_ip_pct >= 0 &&
                   (uint32_t)(now_ip - s_last_good_ip_ms) <= (uint32_t)W20_BAT_IP_HOLD_MS) {
            src = W20_BAT_SRC_IP5356;
            raw_pct = s_last_good_ip_pct;
        }
    }
    if (raw_pct < 0)
#endif
#if USE_CW2217_GAUGE
    if (cwUi >= 0) {
        src = W20_BAT_SRC_CW2217;
        raw_pct = cwUi;
        const int floor =
            w20_battery_near_full_floor_pct(mv, raw_pct, ipPct, heavy, ibat, have_ibat, on_charge);
        if (floor > raw_pct)
            raw_pct = floor;
    } else
#endif
    {
        const int lutRaw = w20_battery_lut_raw(mv, heavy);
        if (lutRaw >= 0) {
            if (s_ui_src != W20_BAT_SRC_V_LUT && s_ui_display >= 0 && lutRaw <= 5 && s_ui_display > 15) {
                s_ui_display = clamp_pct(lutRaw);
                s_ui_slew_ms = millis();
            }
            src = W20_BAT_SRC_V_LUT;
            raw_pct = clamp_pct(lutRaw);
        }
    }

    const bool cap99 = w20_battery_charge_cap_99_active();
    if (cap99 && raw_pct > (int)W20_BAT_SOC_CHARGE_CAP_PCT)
        raw_pct = (int)W20_BAT_SOC_CHARGE_CAP_PCT;

#if W20_BAT_SOC_CHG_RATCHET
    /* 充电棘轮：不低于插电时显示；估计超过地板后再升 */
    if (on_charge && raw_pct >= 0) {
        if (s_chg_ui_floor < 0) {
            if (s_ui_display >= 0)
                s_chg_ui_floor = s_ui_display;
            else
                s_chg_ui_floor = raw_pct;
        }
        if (raw_pct > s_chg_ui_floor)
            s_chg_ui_floor = raw_pct;
        else if (raw_pct < s_chg_ui_floor)
            raw_pct = s_chg_ui_floor;
    } else if (!on_charge) {
        s_chg_ui_floor = -1;
    }
#endif

    /* 电压已到关机点：强制目标 0，并立刻对齐显示，禁止带着中高 % 进自动关机 */
    if (mv > 0 && mv <= (int)W20_BAT_SHUTDOWN_FAILSAFE_MV) {
        raw_pct = 0;
#if W20_BAT_SOC_CHG_RATCHET
        s_chg_ui_floor = -1;
#endif
        if (s_ui_display > (int)W20_BAT_SHUTDOWN_PCT) {
            s_ui_display = 0;
            s_ui_slew_ms = millis();
        }
    }

    int pct = -1;
    if (raw_pct >= 0) {
        const int fullRef = (src == W20_BAT_SRC_V_LUT) ? -1 : raw_pct;
        /* 近满小幅跳变（CW/对齐后）；其余 slew */
        if (s_ui_display >= 0 && raw_pct > s_ui_display &&
            (src == W20_BAT_SRC_CW2217 || src == W20_BAT_SRC_IP5356) &&
            raw_pct >= (int)W20_BAT_NEAR_FULL_MIN_CW_PCT &&
            (raw_pct - s_ui_display) >= (int)W20_BAT_NEAR_FULL_LAG_PCT &&
            (raw_pct - s_ui_display) <= (int)W20_BAT_NEAR_FULL_MAX_LIFT_PCT &&
            (mv >= (int)W20_BAT_NEAR_FULL_MV_95 || on_charge)) {
            s_ui_display = raw_pct;
            s_ui_slew_ms = millis();
            pct = w20_battery_apply_full_hysteresis(raw_pct, fullRef, cap99);
        } else {
#if W20_BAT_SOC_CHG_RATCHET
            if (on_charge && s_ui_display >= 0 && raw_pct < s_ui_display)
                raw_pct = s_ui_display;
#endif
            pct = w20_battery_slew_toward(raw_pct);
            pct = w20_battery_apply_full_hysteresis(pct, fullRef, cap99);
        }
    }

    s_ui_src = src;
#if W20_LOG_BAT
    const int ipReadOk = (ipPct >= 0) ? 1 : 0;
    w20_battery_soc_src_log_if_changed(src, pct, ipPct, cwRaw, ipPresent, ipReadOk, cwPresent);
    w20_battery_pct_jump_log_if_needed(src, pct, ipPct, cwRaw);
#else
    (void)ipPresent;
    (void)cwPresent;
    (void)ipPct;
    (void)cwRaw;
#endif
    return pct;
}

class W20BatteryLevel : public HasBatteryLevel
{
  public:
    int getBatteryPercent() override { return w20_battery_read_percent(); }

    uint16_t getBattVoltage() override
    {
        /* 电压优先 CW（IP 休眠时仍可读），再回退 IP */
#if USE_CW2217_GAUGE
        if (w20_cw2217_is_present()) {
            const int mv = w20_cw2217_read_voltage_mv();
            if (mv > 0)
                return (uint16_t)mv;
        }
#endif
        if (w20_ip5356_is_present()) {
            const int mv = w20_ip5356_read_voltage_mv();
            if (mv > 0)
                return (uint16_t)mv;
        }
        return 0;
    }

    bool isBatteryConnect() override
    {
        if (w20_ip5356_is_present())
            return true;
#if USE_CW2217_GAUGE
        if (w20_cw2217_is_present())
            return true;
#endif
        return false;
    }

    bool isVbusIn() override
    {
        IP5356::Status st;
        if (!w20_ip5356_read_status(st))
            return false;
        return st.inputPresent;
    }

    bool isCharging() override
    {
        IP5356::Status st;
        if (!w20_ip5356_read_status(st))
            return false;
        return st.charging;
    }
};

static W20BatteryLevel s_battery;

#if defined(ARDUINO_ARCH_ESP32)
/** 跨复位保留（棕断/WDT 常能保住；整轨掉电/USB 塌通常清掉）→ 用来区分供电切断 vs 软件挂死。 */
#define W20_PWR_ALIVE_MAGIC 0x57425052u /* 'WBPR' */
typedef struct {
    uint32_t magic;
    uint32_t millis;
    int16_t ui_pct;
    int16_t vbat_mv;
    uint8_t key_sleep;
    uint8_t pad[3];
} w20_pwr_alive_t;
RTC_NOINIT_ATTR static w20_pwr_alive_t s_pwr_alive;

#if W20_BAT_BOD_LATCH
/** 棕断弱电锁存：跨 BROWNOUT/deep-sleep 保留，充电清除。 */
#define W20_BOD_LATCH_MAGIC 0x57424F44u /* 'WBOD' */
typedef struct {
    uint32_t magic;
    uint16_t latch_mv;   /* learned + margin */
    int16_t learned_mv;  /* 棕断前 last_alive VBAT */
    uint8_t locked;      /* 1=弱电锁存有效 */
    uint8_t pad[3];
} w20_bod_latch_t;
RTC_NOINIT_ATTR static w20_bod_latch_t s_bod_latch;
#endif

static bool s_pwr_diag_logged = false;

static const char *w20_esp_rst_name(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "OTHER";
    }
}

static void w20_pwr_diag_boot_log(void)
{
    if (s_pwr_diag_logged)
        return;
    s_pwr_diag_logged = true;

    const esp_reset_reason_t er = esp_reset_reason();
    const RESET_REASON hw0 = rtc_get_reset_reason(0);
    const bool had = (s_pwr_alive.magic == W20_PWR_ALIVE_MAGIC);

    /* 不跟 W20_LOG_BAT：掉电后重连串口必须立刻能看见。ESP32 Arduino 无 ESP.getResetReason()。 */
    Serial.printf("[PWR] boot esp_rst=%d(%s) rtc0=%d\n", (int)er, w20_esp_rst_name(er), (int)hw0);
    if (had) {
        Serial.printf("[PWR] last_alive t=%lums ui=%d%% vbat=%dmV key_sleep=%d (RTC kept)\n",
                      (unsigned long)s_pwr_alive.millis, (int)s_pwr_alive.ui_pct, (int)s_pwr_alive.vbat_mv,
                      (int)s_pwr_alive.key_sleep);
    } else {
        Serial.println(F("[PWR] last_alive none (RTC wiped)"));
    }

    if (er == ESP_RST_BROWNOUT || hw0 == RTCWDT_BROWN_OUT_RESET) {
        Serial.println(F("[PWR] VERDICT: BROWNOUT — supply sag under wireless charge/discharge"));
    } else if (er == ESP_RST_TASK_WDT || er == ESP_RST_INT_WDT || er == ESP_RST_WDT) {
        Serial.println(F("[PWR] VERDICT: WATCHDOG — software hang then reset"));
    } else if (!had && (er == ESP_RST_POWERON || er == ESP_RST_UNKNOWN)) {
        Serial.println(F("[PWR] VERDICT: COLD POWER-ON / hard rail cut (USB drop fits this)"));
    } else if (had) {
        Serial.println(F("[PWR] VERDICT: reset with RTC retained — see esp_rst above"));
    } else {
        Serial.println(F("[PWR] VERDICT: unclear — see esp_rst / rtc0 above"));
    }

#if W20_BAT_BOD_LATCH
    if (s_bod_latch.magic == W20_BOD_LATCH_MAGIC && s_bod_latch.locked) {
        Serial.printf("[PWR] bod_latch ON learned=%dmV latch<=%dmV\n", (int)s_bod_latch.learned_mv,
                      (int)s_bod_latch.latch_mv);
    }
#endif
}

static void w20_pwr_alive_kick(void)
{
    s_pwr_alive.magic = W20_PWR_ALIVE_MAGIC;
    s_pwr_alive.millis = millis();
    s_pwr_alive.ui_pct = (int16_t)(s_ui_display >= 0 ? s_ui_display : -1);
    s_pwr_alive.vbat_mv = (int16_t)((s_last_good_vbat_mv > 0 && s_last_good_vbat_mv < 32767) ? s_last_good_vbat_mv : -1);
    s_pwr_alive.key_sleep = s_key_sleep_mode ? 1 : 0;
}

#if W20_BAT_BOD_LATCH
static bool w20_bod_latch_valid(void)
{
    return s_bod_latch.magic == W20_BOD_LATCH_MAGIC && s_bod_latch.locked != 0;
}

static void w20_bod_latch_clear(const char *why)
{
    if (!w20_bod_latch_valid()) {
        s_bod_latch.magic = 0;
        s_bod_latch.locked = 0;
        return;
    }
    Serial.printf("[PWR] bod_latch CLEAR (%s) was learned=%dmV latch=%dmV\n", why ? why : "?",
                  (int)s_bod_latch.learned_mv, (int)s_bod_latch.latch_mv);
    s_bod_latch.magic = 0;
    s_bod_latch.locked = 0;
    s_bod_latch.latch_mv = 0;
    s_bod_latch.learned_mv = -1;
}

static void w20_bod_latch_learn(int learned_mv)
{
    int latch = learned_mv + (int)W20_BAT_BOD_LATCH_MARGIN_MV;
    const int cap = (int)W20_BAT_BOD_LEARN_MAX_MV + (int)W20_BAT_BOD_LATCH_MARGIN_MV;
    if (latch > cap)
        latch = cap;
    if (latch < (int)W20_BAT_SHUTDOWN_FAILSAFE_MV)
        latch = (int)W20_BAT_SHUTDOWN_FAILSAFE_MV;

    s_bod_latch.magic = W20_BOD_LATCH_MAGIC;
    s_bod_latch.locked = 1;
    s_bod_latch.learned_mv = (int16_t)learned_mv;
    s_bod_latch.latch_mv = (uint16_t)latch;
    Serial.printf("[PWR] bod_latch LEARN v=%dmV -> latch<=%dmV (halt until charge)\n", learned_mv, latch);
}

static bool w20_battery_probe_powered(void)
{
    if (w20_ip5356_is_present()) {
        IP5356::Status st;
        if (w20_ip5356_read_status(st) && (st.inputPresent || st.charging))
            return true;
    }
    return false;
}

static int w20_battery_probe_vbat_quick(void)
{
#if USE_CW2217_GAUGE
    if (w20_cw2217_is_present()) {
        const int mv = w20_cw2217_read_voltage_mv();
        if (mv >= (int)W20_BAT_CW_VALID_MIN_MV)
            return mv;
    }
#endif
    if (w20_ip5356_is_present()) {
        const int mv = w20_ip5356_read_voltage_mv();
        if (mv >= (int)W20_BAT_IP_VALID_MIN_MV)
            return mv;
    }
    return -1;
}

static void w20_bod_latch_try_clear(int cur_mv, bool powered)
{
    if (!w20_bod_latch_valid())
        return;
    if (powered) {
        w20_bod_latch_clear("charge/vbus");
        return;
    }
    if (cur_mv >= (int)W20_BAT_BOD_CLEAR_MIN_MV) {
        w20_bod_latch_clear("vbat recovered");
        return;
    }
}
#endif /* W20_BAT_BOD_LATCH */
#endif /* ARDUINO_ARCH_ESP32 */

bool w20_battery_brownout_latch_active(void)
{
#if defined(ARDUINO_ARCH_ESP32) && W20_BAT_BOD_LATCH
    return w20_bod_latch_valid();
#else
    return false;
#endif
}

void w20_battery_brownout_latch_poll_clear_on_charge(void)
{
#if defined(ARDUINO_ARCH_ESP32) && W20_BAT_BOD_LATCH
    if (!w20_bod_latch_valid())
        return;
    const bool powered = w20_battery_probe_powered();
    const int mv = w20_battery_read_vbat_mv();
    w20_bod_latch_try_clear(mv, powered);
#endif
}

bool w20_battery_boot_lowbatt_should_halt(void)
{
#if !(defined(ARDUINO_ARCH_ESP32) && W20_BAT_BOD_LATCH)
    return false;
#else
    w20_pwr_diag_boot_log();

    /* 轻量探测：CW 电压 + IP 充电态（失败不阻塞） */
#if USE_CW2217_GAUGE
    (void)w20_cw2217_boot_init();
#endif
#if USE_IP5356_CHARGER
    (void)w20_ip5356_boot_init();
#endif

    const int cur_mv = w20_battery_probe_vbat_quick();
    if (cur_mv > 0)
        s_last_good_vbat_mv = cur_mv;

    const bool powered = w20_battery_probe_powered();
    w20_bod_latch_try_clear(cur_mv, powered);
    if (powered) {
        Serial.printf("[PWR] lowbatt gate: powered, skip halt (v=%dmV)\n", cur_mv);
        return false;
    }

    const esp_reset_reason_t er = esp_reset_reason();
    const RESET_REASON hw0 = rtc_get_reset_reason(0);
    const bool is_bod = (er == ESP_RST_BROWNOUT || hw0 == RTCWDT_BROWN_OUT_RESET);
    const bool had_alive = (s_pwr_alive.magic == W20_PWR_ALIVE_MAGIC);
    const int last_mv = had_alive ? (int)s_pwr_alive.vbat_mv : -1;
    const int last_ui = had_alive ? (int)s_pwr_alive.ui_pct : -1;

    if (is_bod && last_mv >= (int)W20_BAT_CW_VALID_MIN_MV && last_mv <= (int)W20_BAT_BOD_LEARN_MAX_MV) {
        /* 电压已偏低，或 UI 已见底：学习锁存 */
        if (last_ui <= 1 || last_mv <= (int)W20_BAT_BOD_LEARN_MAX_MV) {
            w20_bod_latch_learn(last_mv);
        }
    }

    if (!w20_bod_latch_valid())
        return false;

    /* 锁存有效且未充电：一律走低电关机（打断闪屏环） */
    Serial.printf("[PWR] lowbatt gate HALT locked latch=%dmV cur=%dmV learned=%dmV\n",
                  (int)s_bod_latch.latch_mv, cur_mv, (int)s_bod_latch.learned_mv);
    return true;
#endif
}

bool w20PowerBatteryInit(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    w20_pwr_diag_boot_log();
#endif
    const bool ip = w20_ip5356_boot_init();
#if USE_CW2217_GAUGE
    const bool cw = w20_cw2217_boot_init();
    if (!ip && !cw)
        return false;
#else
    if (!ip)
        return false;
#endif
    powerSetBatteryLevel(&s_battery);
    powerRefreshStatus();
#if defined(ARDUINO_ARCH_ESP32)
    w20_pwr_alive_kick();
#if W20_BAT_BOD_LATCH
    w20_battery_brownout_latch_poll_clear_on_charge();
#endif
#endif
    return true;
}

void w20_power_battery_on_ip5356_ready(void)
{
    if (!w20_ip5356_is_present())
        return;
    powerSetBatteryLevel(&s_battery);
    powerRefreshStatus();
#if defined(ARDUINO_ARCH_ESP32) && W20_BAT_BOD_LATCH
    w20_battery_brownout_latch_poll_clear_on_charge();
#endif
    LOG_INFO("IP5356 late-ready: battery provider refreshed");
}

void w20_battery_dual_soc_log_poll(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    {
        static uint32_t s_alive_ms = 0;
        const uint32_t now_alive = millis();
        if (s_alive_ms == 0 || (uint32_t)(now_alive - s_alive_ms) >= 1000u) {
            s_alive_ms = now_alive;
            w20_pwr_alive_kick();
        }
    }
#if W20_BAT_BOD_LATCH
    {
        static uint32_t s_bod_clr_ms = 0;
        const uint32_t now_clr = millis();
        if (s_bod_clr_ms == 0 || (uint32_t)(now_clr - s_bod_clr_ms) >= 2000u) {
            s_bod_clr_ms = now_clr;
            w20_battery_brownout_latch_poll_clear_on_charge();
        }
    }
#endif
#endif
#if W20_LOG_BAT
    static uint32_t s_hb_ms = 0;
    static uint32_t s_cal_ms = 0;
    static bool s_cal_hdr = false;
    const uint32_t now = millis();

    if (!s_cal_hdr) {
        s_cal_hdr = true;
        W20_BAT_LOGLN(F("[BATCSV] t_ms,ui_pct,ui_src,key_sleep,ip_pct,ip_mv,ip_ibat_ma,ip_iin_ma,chg,vbus,"
                        "cw_pct,cw_mv,cw_soh,bias"));
    }
    if (s_cal_ms == 0 || (uint32_t)(now - s_cal_ms) >= W20_BAT_CAL_CSV_MS) {
        s_cal_ms = now;
        const int uiPct = w20_battery_read_percent();
        const int ipPct = w20_ip5356_is_present() ? w20_ip5356_read_percent() : -1;
        const int ipMv = w20_ip5356_is_present() ? w20_ip5356_read_voltage_mv() : -1;
        const int ibat = w20_ip5356_is_present() ? w20_ip5356_read_bat_current_ma() : 0;
        const int iin = w20_ip5356_is_present() ? w20_ip5356_read_input_current_ma() : 0;
        int chg = 0, vbus = 0;
        if (w20_ip5356_is_present()) {
            IP5356::Status st;
            if (w20_ip5356_read_status(st)) {
                chg = st.charging ? 1 : 0;
                vbus = st.inputPresent ? 1 : 0;
            }
        }
#if USE_CW2217_GAUGE
        const int cwPct = w20_cw2217_is_present() ? w20_cw2217_read_percent() : -1;
        const int cwMv = w20_cw2217_is_present() ? w20_cw2217_read_voltage_mv() : -1;
        const int cwSoh = w20_cw2217_is_present() ? w20_cw2217_read_soh_percent() : -1;
#else
        const int cwPct = -1, cwMv = -1, cwSoh = -1;
#endif
        W20_BAT_LOG("[BATCSV] %lu,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", (unsigned long)now, uiPct,
                    w20_battery_ui_src_name(), w20_battery_key_sleep_mode() ? 1 : 0, ipPct, ipMv, ibat, iin, chg, vbus,
                    cwPct, cwMv, cwSoh, w20_battery_soc_align_bias());
    }

    if (s_hb_ms != 0 && (uint32_t)(now - s_hb_ms) < W20_BAT_LOG_HB_MS)
        return;
    s_hb_ms = now;

    const bool ipOk = w20_ip5356_is_present();
    const int ipPct = ipOk ? w20_ip5356_read_percent() : -1;
    const int ipMv = ipOk ? w20_ip5356_read_voltage_mv() : -1;
    const int uiPct = w20_battery_read_percent();
#if USE_CW2217_GAUGE
    const int cwX100 = w20_cw2217_is_present() ? w20_cw2217_read_percent_x100() : -1;
    const int cwMv = w20_cw2217_is_present() ? w20_cw2217_read_voltage_mv() : -1;
    const int cwSoh = w20_cw2217_is_present() ? w20_cw2217_read_soh_percent() : -1;
    if (cwX100 >= 0) {
        W20_BAT_LOG("[BAT] UI=%s %d%% | IP5356 %d%% %dmV | CW2217 %d.%02d%% %dmV SOH=%d | bias=%+d lat=%d key_sleep=%d t=%lu\n",
                    w20_battery_ui_src_name(), uiPct, ipPct, ipMv, cwX100 / 100, cwX100 % 100, cwMv, cwSoh,
                    w20_battery_soc_align_bias(),
#if W20_BAT_SOC_ALIGN
                    s_vbus_latched ? 1 : 0,
#else
                    0,
#endif
                    s_key_sleep_mode ? 1 : 0, (unsigned long)now);
    } else {
        W20_BAT_LOG("[BAT] UI=%s %d%% | IP5356 %d%% %dmV | CW2217 absent | t=%lu\n", w20_battery_ui_src_name(), uiPct,
                    ipPct, ipMv, (unsigned long)now);
    }
#else
    W20_BAT_LOG("[BAT] UI=%s %d%% | IP5356 %d%% %dmV | CW2217 disabled | t=%lu\n", w20_battery_ui_src_name(), uiPct,
                ipPct, ipMv, (unsigned long)now);
#endif
#endif /* W20_LOG_BAT */
}

static bool s_auto_shutdown_pending = false;
static uint32_t s_v_low_since_ms = 0;

bool w20_battery_auto_shutdown_pending(void)
{
    return s_auto_shutdown_pending;
}

void w20_battery_auto_shutdown_poll(void)
{
#if !W20_BAT_AUTO_SHUTDOWN
    return;
#else
    if (s_auto_shutdown_pending)
        return;

    const uint32_t now = millis();

    /* 充电/有 VBUS：不自动关机，并清计时 */
    bool powered = false;
    if (w20_ip5356_is_present()) {
        IP5356::Status st;
        if (w20_ip5356_read_status(st))
            powered = st.inputPresent || st.charging;
    }
    if (powered) {
        s_v_low_since_ms = 0;
        return;
    }

    /* 刷新 SOC：V≤FAILSAFE 时会强制 UI=0；关机判定本身只看电压 */
    const int uiPct = w20_battery_read_percent();
    const int mv = w20_battery_read_vbat_mv();

    int ibat = 0;
    bool have_ibat = false;
    if (w20_ip5356_is_present()) {
        const int ipPct = w20_ip5356_read_percent();
        if (ipPct >= 0) {
            ibat = w20_ip5356_read_bat_current_ma();
            have_ibat = true;
        } else if (s_last_ibat_ms != 0 && (uint32_t)(now - s_last_ibat_ms) < 60000u) {
            ibat = s_last_ibat_ma;
            have_ibat = true;
        }
    }

    const int ibat_mag = (ibat < 0) ? -ibat : ibat;
    const bool heavy = have_ibat && ibat_mag >= (int)W20_BAT_SHUTDOWN_FAILSAFE_MAX_IBAT_MA;

    /* 仅电压：≤3.0V 且非大电流压降，持续 hold 后关机 */
    if (mv > 0 && mv <= (int)W20_BAT_SHUTDOWN_FAILSAFE_MV && !heavy) {
        if (s_v_low_since_ms == 0)
            s_v_low_since_ms = now;
        else if ((uint32_t)(now - s_v_low_since_ms) >= W20_BAT_SHUTDOWN_FAILSAFE_HOLD_MS) {
            s_auto_shutdown_pending = true;
            Serial.printf("[BAT] auto-shutdown: V=%dmV <= %d for %lums ui=%d%% t=%lu\n", mv,
                          (int)W20_BAT_SHUTDOWN_FAILSAFE_MV, (unsigned long)W20_BAT_SHUTDOWN_FAILSAFE_HOLD_MS, uiPct,
                          (unsigned long)now);
            return;
        }
    } else {
        s_v_low_since_ms = 0;
    }
#endif
}

#endif /* W20_MESHNOLOGY */
