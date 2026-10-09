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

static void w20_battery_align_learn(int ip_pct, int cw_ui, bool heavy)
{
    /* KEY 休眠 / 大电流 IR：IP 与 CW 都不可靠，勿学 */
    if (ip_pct < 0 || cw_ui < 0 || s_key_sleep_mode || heavy)
        return;
    /* 见底时 err≈IP−0 会把 bias 顶满，禁止学习 */
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT)
        return;
    if (!w20_ip5356_is_present())
        return;
    const uint32_t now = millis();
    if (s_align_learn_ms != 0 && (uint32_t)(now - s_align_learn_ms) < (uint32_t)W20_BAT_SOC_ALIGN_LEARN_MS)
        return;
    s_align_learn_ms = now;

    const int err = ip_pct - cw_ui;
    const int div = (W20_BAT_SOC_ALIGN_EMA_DIV > 0) ? (int)W20_BAT_SOC_ALIGN_EMA_DIV : 8;
    int delta = (err - s_align_bias) / div;
    if (delta == 0 && err != s_align_bias)
        delta = (err > s_align_bias) ? 1 : -1;
    s_align_bias += delta;
    if (s_align_bias > (int)W20_BAT_SOC_ALIGN_BIAS_MAX)
        s_align_bias = (int)W20_BAT_SOC_ALIGN_BIAS_MAX;
    if (s_align_bias < -(int)W20_BAT_SOC_ALIGN_BIAS_MAX)
        s_align_bias = -(int)W20_BAT_SOC_ALIGN_BIAS_MAX;
}

static int w20_battery_apply_align(int cw_ui)
{
    if (cw_ui < 0)
        return -1;
    int bias = s_align_bias;
    /* 见底时正 bias 不参与，UI 可到 0（即使 attenuate 尚未跑） */
    if (cw_ui <= (int)W20_BAT_SOC_ALIGN_EMPTY_CW_PCT && bias > 0)
        bias = 0;
    return clamp_pct(cw_ui + bias);
}
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
    cwUi = w20_battery_apply_align(cwUi);
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
 */
static int w20_battery_near_full_floor_pct(int mv, int cw_pct, int ip_pct, bool heavy_load, int ibat_ma,
                                          bool have_ibat)
{
#if !W20_BAT_NEAR_FULL_FLOOR
    (void)mv;
    (void)cw_pct;
    (void)ip_pct;
    (void)heavy_load;
    (void)ibat_ma;
    (void)have_ibat;
    return -1;
#else
    (void)ip_pct; /* 不再用 IP% 抬地板 */
    if (mv < (int)W20_BAT_NEAR_FULL_MV_90 || heavy_load)
        return -1;
    if (cw_pct < 0 || cw_pct < (int)W20_BAT_NEAR_FULL_MIN_CW_PCT)
        return -1;
    if (have_ibat) {
        const int mag = (ibat_ma < 0) ? -ibat_ma : ibat_ma;
        if (mag > (int)W20_BAT_NEAR_FULL_MAX_IBAT_MA)
            return -1;
    }

    int floor = 90;
    if (mv >= (int)W20_BAT_NEAR_FULL_MV_99)
        floor = (int)W20_BAT_SOC_CHARGE_CAP_PCT; /* 地板最多 99；对号亮时由 hysteresis 升到 100 */
    else if (mv >= (int)W20_BAT_NEAR_FULL_MV_95)
        floor = 95;

    if ((floor - cw_pct) < (int)W20_BAT_NEAR_FULL_LAG_PCT)
        return -1;
    /* 相对 CW 限幅抬升 */
    const int capped = cw_pct + (int)W20_BAT_NEAR_FULL_MAX_LIFT_PCT;
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
    if (ipPresent) {
        ipPct = w20_ip5356_read_percent();
        ibat = w20_ip5356_read_bat_current_ma();
        have_ibat = true;
        s_last_ibat_ma = ibat;
        s_last_ibat_ms = millis();
    }

    const bool heavy = w20_battery_want_heavy_lut(ibat, have_ibat);
    const int mv = w20_battery_read_vbat_mv();

    bool vbus = false;
    if (ipPresent) {
        IP5356::Status st;
        if (w20_ip5356_read_status(st))
            vbus = st.inputPresent;
    }
#if USE_CW2217_GAUGE
    int cwUi = w20_battery_cw_scale_ui(cwRaw);
#if W20_BAT_SOC_ALIGN && !W20_BAT_UI_SRC_IP5356
    /* 轻载学 bias；休眠/大电流停学；见底/低 V 清正 bias */
    if (ipPct >= 0)
        w20_battery_align_learn(ipPct, cwUi, heavy);
    w20_battery_align_attenuate(cwUi, mv);
    cwUi = w20_battery_apply_align(cwUi);
#endif
#else
    const int cwUi = -1;
#endif

    /* 拔电边沿：主源未到近满则清满电锁，禁止假 100% */
    if (s_vbus_was && !vbus) {
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
    }
    s_vbus_was = vbus;

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
        const int floor = w20_battery_near_full_floor_pct(mv, raw_pct, ipPct, heavy, ibat, have_ibat);
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

    /* 电压已到关机点：强制目标 0，并立刻对齐显示，禁止带着中高 % 进自动关机 */
    if (mv > 0 && mv <= (int)W20_BAT_SHUTDOWN_FAILSAFE_MV) {
        raw_pct = 0;
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
            (mv >= (int)W20_BAT_NEAR_FULL_MV_95 || vbus)) {
            s_ui_display = raw_pct;
            s_ui_slew_ms = millis();
            pct = w20_battery_apply_full_hysteresis(raw_pct, fullRef, cap99);
        } else {
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

bool w20PowerBatteryInit(void)
{
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
    return true;
}

void w20_power_battery_on_ip5356_ready(void)
{
    if (!w20_ip5356_is_present())
        return;
    powerSetBatteryLevel(&s_battery);
    powerRefreshStatus();
    LOG_INFO("IP5356 late-ready: battery provider refreshed");
}

void w20_battery_dual_soc_log_poll(void)
{
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
        W20_BAT_LOG("[BAT] UI=%s %d%% | IP5356 %d%% %dmV | CW2217 %d.%02d%% %dmV SOH=%d | bias=%+d key_sleep=%d t=%lu\n",
                    w20_battery_ui_src_name(), uiPct, ipPct, ipMv, cwX100 / 100, cwX100 % 100, cwMv, cwSoh,
                    w20_battery_soc_align_bias(), s_key_sleep_mode ? 1 : 0, (unsigned long)now);
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
