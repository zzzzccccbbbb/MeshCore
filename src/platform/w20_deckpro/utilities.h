/**
 * @file      utilities.h
 * @author    ShallowGreen123
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-05-27
 */
#pragma once

#define UI_T_DECK_PRO_VERSION    "v1.3-261024"  // Software version
#define BOARD_T_DECK_PRO_VERSION "v2.0-260812"  // Hardware version

// Serial
#define SerialMon   Serial      // 
#define SerialAT    Serial1     // 
#define SerialGPS   Serial2     // 

// I2C Addresses (keep legacy for compatibility; enable/disable via feature toggles below)
// FT6X36 family (FT6336U/FT6236/FT3267) typical I2C address is 0x38
#define BOARD_I2C_ADDR_TOUCH       0x58 // CST3530 L5W41
#define BOARD_I2C_ADDR_LTR_553ALS  0x23 // LTR_553ALS (ambient light) [disabled by default]
#define BOARD_I2C_ADDR_GYROSCOPDE  0x28 // BHI260AP (gyro)            [disabled by default]
#define BOARD_I2C_ADDR_KEYBOARD    0x34 // TCA8418 (keypad)           [disabled, replaced by GPIO keys]
#define BOARD_I2C_ADDR_BQ25896     0x6B // Charger (legacy, not populated)

// IP5356M：与 LED1/2/3 复用为 SCL/SDA/INT 后自动进入 I2C 模式（LED 灯显关闭）。
// 7 位从机地址固定 0x75（无 ADDR 引脚）；8 位写 0xEA / 读 0xEB。
#define BOARD_I2C_ADDR_IP5356      0x75
// CW2217EAAD-DS 电量计。7 位 0x64；8 位写 0xC8 / 读 0xC9。INT_N 仅测试点。
#ifndef BOARD_I2C_ADDR_CW2217
#define BOARD_I2C_ADDR_CW2217      0x64
#endif

// Feature toggles (set to 1 to enable, 0 to disable)
#define USE_TCA8418_KEYPAD   0   // Replaced by GPIO keys: BOOT (GPIO0) + LED_KEY (GPIO48); RST is EN pin
#define USE_BHI260AP_GYRO    0
#define USE_LTR_553ALS       0
#define USE_BQ25896_CHARGER  0   // Replaced by IP5356
/* IP5356：充电/Type-C/电流/Finish；开机 I2C 探测 @0x75。 */
#define USE_IP5356_CHARGER 1
/* CW2217：UI 电量百分比固定走此计；不可读时才电压 LUT 兜底。 */
#define USE_CW2217_GAUGE   1
/**
 * 调试串口开关（默认全关，见 w20_dbg_log.h）：
 * - 电量+KEY：W20_LOG_BAT → W20_BAT_LOG*
 * - WiFi（配网/Config/OTA/菜单）：W20_LOG_WIFI → W20_WIFI_LOG*
 * - UI/手势/EPD：W20_LOG_UI → W20_UI_LOG*
 * - 触控/Setting：默认跟随 UI；可单独 W20_LOG_TOUCH / W20_LOG_SETTING
 * - GPS / LoRa / LED键 / SOS：W20_LOG_GPS / LORA / LED_KEY / SOS
 * 失败/报错打印不走这些宏，始终输出。
 */
/** 电量心跳间隔（W20_LOG_BAT=1 时双计 UI/IP/CW 一行）。 */
#ifndef W20_BAT_LOG_HB_MS
#define W20_BAT_LOG_HB_MS 10000u
#endif
/** UI 百分比相对上次变化 ≥ 该值时打印 [BAT] pct jump（需 W20_LOG_BAT）。 */
#ifndef W20_BAT_PCT_JUMP_LOG
#define W20_BAT_PCT_JUMP_LOG 3
#endif
/**
 * UI% 策略（默认 CW，IP 仅校准）：
 * - 主源始终 CW2217：先线性拉伸（FULL_RAW），再加 IP 学到的 bias（见 W20_BAT_SOC_ALIGN）
 * - IP 在线且非 KEY 休眠、非大电流：分阶段学 bias≈IP%−CW拉伸%（分歧大则更慢）
 * - 充电（VBUS/charging）：只学正 bias（向 IP 攀升）；目标封顶 CHARGE_CAP（未 Finish 勿信 IP=100）；
 *   忽略负 bias；UI 棘轮只升不降；大电流时仍可慢爬（强制大分歧档）
 * - 插电态锁存：IP 短暂 I2C 失败时保持 on_charge，正 bias 不衰（防卡在 70–80%）
 * - CV 尾（小电流+高 V）：加快正学；充电近满电压地板门槛略低
 * - 放电/轻载：逻辑不变（含静载 VBAT、离线衰 bias）
 * - |IP−CW| 或 |V_IP−V_CW| 过大：不猛靠，缓靠；真离线/拔电后 bias 缓衰回 0（跟回 CW）
 * - 静载 VBAT（无快充/快放、非 VBUS、大电流后已回弹）：LUT% 作阶段性软参考，慢纠 bias
 * - 开机/拔电：静载确认 VBAT 后一次性 snap bias+UI（勿慢爬像在充电）
 * - CW 见底 / VBAT 过低：清掉正 bias，避免 UI 卡在 bias_max
 * - CW 故障才电压 LUT；插电封顶 99；对号 = Finish +（拉伸+bias 后）UI≥FINISH_ICON_MIN
 */
/** 1=UI% 直接跟 IP5356（旧试跑）；0=跟 CW+IP 校准（推荐默认）。 */
#ifndef W20_BAT_UI_SRC_IP5356
#define W20_BAT_UI_SRC_IP5356 0
#endif
/**
 * 仅当 W20_BAT_UI_SRC_IP5356=1：IP 读失败后 hold 上次 IP% 的时长。
 */
#ifndef W20_BAT_IP_HOLD_MS
#define W20_BAT_IP_HOLD_MS 60000u
#endif
/**
 * CW 满电线性拉伸：芯片 ~80% → UI 100（约 78%→98%，≥95）。
 * 关闭：W20_BAT_CW_SOC_SCALE=0 或 FULL_RAW≤0 / ≥100。
 */
#ifndef W20_BAT_CW_SOC_SCALE
#define W20_BAT_CW_SOC_SCALE 1
#endif
/** 真满电时 CW 芯片读数；ui = round(cw×100/该值)。 */
#ifndef W20_BAT_CW_SOC_FULL_RAW
#define W20_BAT_CW_SOC_FULL_RAW 80
#endif
/** CW VCELL 低于此值视为未测到电池（勿把 SOC=0 当有效主源）。 */
#ifndef W20_BAT_CW_VALID_MIN_MV
#define W20_BAT_CW_VALID_MIN_MV 2000
#endif
/**
 * IP5356 VBAT 有效下限。KEY 低/休眠时常见毛刺 ~0.6V，绝不可当真低压把 UI 打成 0%。
 * 真低电关机仍靠 ≤ W20_BAT_SHUTDOWN_FAILSAFE_MV(3.0V) 的稳定读数。
 */
#ifndef W20_BAT_IP_VALID_MIN_MV
#define W20_BAT_IP_VALID_MIN_MV 2500
#endif
/** CW 读数持续无效后，限频尝试 wake/重配（ms）。 */
#ifndef W20_BAT_CW_RECOVER_MS
#define W20_BAT_CW_RECOVER_MS 15000u
#endif
#ifndef W20_BAT_SOC_SLEW_PCT_PER_S
#define W20_BAT_SOC_SLEW_PCT_PER_S 1
#endif
/**
 * 充电棘轮：VBUS/charging 时 UI 禁止因对齐/负 bias 下降；
 * 仅当估计值超过当前显示才上升（避免插电后跟偏低 IP% 往下掉）。
 */
#ifndef W20_BAT_SOC_CHG_RATCHET
#define W20_BAT_SOC_CHG_RATCHET 1
#endif
/** 近满数字滞回：非充电时进入 100 的阈值 / 退出下限。 */
#ifndef W20_BAT_SOC_FULL_ENTER
#define W20_BAT_SOC_FULL_ENTER 99
#endif
#ifndef W20_BAT_SOC_FULL_EXIT
#define W20_BAT_SOC_FULL_EXIT 95
#endif
/** 插电且尚未充满（对号未亮）时 UI% 上限；对号亮则显示 100。 */
#ifndef W20_BAT_SOC_CHARGE_CAP_PCT
#define W20_BAT_SOC_CHARGE_CAP_PCT 99
#endif
/**
 * 充满对号最低「映射后」CW UI%。须 IP Finish 且 UI%≥此值才亮对号，
 * 同时 UI 数字升为 100（插电亦可：100 + 对号 + 满格）。
 * 门槛看「拉伸+IP bias 后」的 UI%；配合 FULL_RAW=80 与 ALIGN，近满应能到 ≥90。
 */
#ifndef W20_BAT_FINISH_ICON_MIN_CW_PCT
#define W20_BAT_FINISH_ICON_MIN_CW_PCT 90
#endif
/**
 * CW 近满偏低兼容：仅当（映射后）CW 已接近满电、电压也够高时，小幅抬升 UI。
 * 禁止「CW 中段 + 拔电电压回弹 → 地板 99 → 锁 100」。
 */
#ifndef W20_BAT_NEAR_FULL_FLOOR
#define W20_BAT_NEAR_FULL_FLOOR 1
#endif
#ifndef W20_BAT_NEAR_FULL_MV_99
#define W20_BAT_NEAR_FULL_MV_99 4180
#endif
#ifndef W20_BAT_NEAR_FULL_MV_95
#define W20_BAT_NEAR_FULL_MV_95 4140
#endif
#ifndef W20_BAT_NEAR_FULL_MV_90
#define W20_BAT_NEAR_FULL_MV_90 4100
#endif
/**
 * 拔电静息：VBAT≥此 mV → UI 地板至少 90%（满电约 4.2V，≥4.10V 不应掉回 CW 裸拉伸）。
 */
#ifndef W20_BAT_NEAR_FULL_MV_REST_90
#define W20_BAT_NEAR_FULL_MV_REST_90 4100
#endif
/**
 * 近满静息地板退出阈值（mV）：进入用 REST_90，退出用本值，避免 4100↔4097 抖掉回 CW。
 */
#ifndef W20_BAT_NEAR_FULL_MV_REST_90_EXIT
#define W20_BAT_NEAR_FULL_MV_REST_90_EXIT 4000
#endif
/** CW 至少到此（映射后 UI%）才允许电压地板抬升（低于则完全信任 CW）。 */
#ifndef W20_BAT_NEAR_FULL_MIN_CW_PCT
#define W20_BAT_NEAR_FULL_MIN_CW_PCT 80
#endif
/**
 * 充电时近满地板门槛（映射后 UI%）。低于放电门槛，便于 CV 尾靠电压抬一截。
 */
#ifndef W20_BAT_NEAR_FULL_MIN_CW_PCT_CHG
#define W20_BAT_NEAR_FULL_MIN_CW_PCT_CHG 70
#endif
/**
 * 拔电静息近满门槛（映射后 UI%）：CW 拉伸常偏低，门槛留低以便电压地板兜住
 * （勿因 CW≈58 而整段地板失效 → 掉到 76）。
 */
#ifndef W20_BAT_NEAR_FULL_MIN_CW_PCT_REST
#define W20_BAT_NEAR_FULL_MIN_CW_PCT_REST 40
#endif
/** 相对 CW 最多抬升百分点（防拔电 IR 回弹跳变）。 */
#ifndef W20_BAT_NEAR_FULL_MAX_LIFT_PCT
#define W20_BAT_NEAR_FULL_MAX_LIFT_PCT 8
#endif
/**
 * 拔电静息近满：相对 CW 最多抬升（≥4.10V→至少 90，CW 拉伸 50 也要抬得上去）。
 * 仍封顶 95，不直接给 100。
 */
#ifndef W20_BAT_NEAR_FULL_MAX_LIFT_PCT_REST
#define W20_BAT_NEAR_FULL_MAX_LIFT_PCT_REST 50
#endif
/** CW 比电压地板至少低这么多才抬升（避免正常抖动）。 */
#ifndef W20_BAT_NEAR_FULL_LAG_PCT
#define W20_BAT_NEAR_FULL_LAG_PCT 3
#endif
/**
 * 近满地板允许的最大 |ibat|（mA）。超过则不抬（含大电流充电 IR 抬压）。
 */
#ifndef W20_BAT_NEAR_FULL_MAX_IBAT_MA
#define W20_BAT_NEAR_FULL_MAX_IBAT_MA 400
#endif
/**
 * CW 失败走电压 LUT 时：|ibat|≥该值用带载 LUT；否则轻载。
 */
#ifndef W20_BAT_SOC_HEAVY_IBAT_MA
#define W20_BAT_SOC_HEAVY_IBAT_MA 1500
#endif
/** IP 离线后 Finish 锁存最长保持（ms）；仅影响对号，不再改写 UI%。 */
#ifndef W20_BAT_FINISH_LATCH_HOLD_MS
#define W20_BAT_FINISH_LATCH_HOLD_MS 8000u
#endif

/**
 * IP→CW 校准：轻载分阶段学 bias，UI = clamp(CW拉伸 + bias)。
 * 分歧大时慢靠；IP 离线 bias 缓衰；CW≤EMPTY 或 V≤EMPTY_MV 时清正 bias。
 */
#ifndef W20_BAT_SOC_ALIGN
#define W20_BAT_SOC_ALIGN 1
#endif
/** 小分歧（|IP−CW|＜MID）时 EMA 分母；越大越慢。 */
#ifndef W20_BAT_SOC_ALIGN_EMA_DIV
#define W20_BAT_SOC_ALIGN_EMA_DIV 8
#endif
/** |IP−CW|≥ 此值：中分歧，更慢 EMA。 */
#ifndef W20_BAT_SOC_ALIGN_GAP_MID
#define W20_BAT_SOC_ALIGN_GAP_MID 10
#endif
#ifndef W20_BAT_SOC_ALIGN_EMA_DIV_MID
#define W20_BAT_SOC_ALIGN_EMA_DIV_MID 16
#endif
/** |IP−CW|≥ 此值：大分歧，极慢靠，且每步最多 ±1。 */
#ifndef W20_BAT_SOC_ALIGN_GAP_LARGE
#define W20_BAT_SOC_ALIGN_GAP_LARGE 18
#endif
#ifndef W20_BAT_SOC_ALIGN_EMA_DIV_LARGE
#define W20_BAT_SOC_ALIGN_EMA_DIV_LARGE 32
#endif
/** |V_IP−V_CW|≥ 此 mV：至少按中分歧处理（电压对不上不猛信 IP%）。 */
#ifndef W20_BAT_SOC_ALIGN_V_DISAGREE_MV
#define W20_BAT_SOC_ALIGN_V_DISAGREE_MV 80
#endif
/** |bias| 上限（放电/轻载）；大分歧时也不一次钉死到 IP。 */
#ifndef W20_BAT_SOC_ALIGN_BIAS_MAX
#define W20_BAT_SOC_ALIGN_BIAS_MAX 18
#endif
/**
 * 充电攀升专用 bias 上限：CW 拉伸偏低时仍可靠向 IP，使 Finish+UI≥90 可达。
 * （例：CW 拉伸~65 + bias28 → 93，配合 IP Finish 亮对号。）
 */
#ifndef W20_BAT_SOC_ALIGN_BIAS_MAX_CHG
#define W20_BAT_SOC_ALIGN_BIAS_MAX_CHG 28
#endif
#ifndef W20_BAT_SOC_ALIGN_LEARN_MS
#define W20_BAT_SOC_ALIGN_LEARN_MS 2000u
#endif
/** 大分歧时两次学习最短间隔（比 LEARN_MS 更长）。 */
#ifndef W20_BAT_SOC_ALIGN_LEARN_MS_LARGE
#define W20_BAT_SOC_ALIGN_LEARN_MS_LARGE 5000u
#endif
/** 充电正学：默认用大分歧间隔，避免插电后 UI 猛跳。 */
#ifndef W20_BAT_SOC_ALIGN_LEARN_MS_CHG
#define W20_BAT_SOC_ALIGN_LEARN_MS_CHG 4000u
#endif
/**
 * CV 尾正学间隔：|ibat|≤CV_IBAT 且 V≥CV_MV 时用此值（更快爬向近满）。
 */
#ifndef W20_BAT_SOC_ALIGN_LEARN_MS_CHG_CV
#define W20_BAT_SOC_ALIGN_LEARN_MS_CHG_CV 2000u
#endif
/** CV 尾判定：|ibat| 不超过此 mA（含符号绝对值）。 */
#ifndef W20_BAT_SOC_ALIGN_CV_IBAT_MA
#define W20_BAT_SOC_ALIGN_CV_IBAT_MA 250
#endif
/** CV 尾判定：VBAT/IP 电压至少此 mV。 */
#ifndef W20_BAT_SOC_ALIGN_CV_MV
#define W20_BAT_SOC_ALIGN_CV_MV 4150
#endif
/**
 * IP 读失败后仍视为插电的最长保持（ms）。期间正 bias 不衰、棘轮保留；
 * 成功读到无 VBUS 立即清。真拔电且 IP 仍应答则无此延迟。
 */
#ifndef W20_BAT_SOC_ALIGN_VBUS_HOLD_MS
#define W20_BAT_SOC_ALIGN_VBUS_HOLD_MS 30000u
#endif
/** IP 离线后 bias 向 0 衰减间隔（跟回 CW，避免钉在 -25 假低电）。 */
#ifndef W20_BAT_SOC_ALIGN_DECAY_MS
#define W20_BAT_SOC_ALIGN_DECAY_MS 4000u
#endif
/**
 * 拔电后 VBAT≥此 mV：正 bias 不衰减（勿跟回 CW 裸拉伸）。
 * 默认 3.6V：整段中高电量都信 VBAT 纠偏；电压落到阈值以下才衰回 CW。
 */
#ifndef W20_BAT_SOC_ALIGN_HOLD_BIAS_MV
#define W20_BAT_SOC_ALIGN_HOLD_BIAS_MV 3600
#endif
/**
 * 静载 VBAT 软参考：仅 |ibat| 小、非 VBUS、大电流结束后已 settle 时，
 * 用轻载 LUT(VBAT) 作阶段性纠正（不替代 CW/IP，只慢拉偏大的 UI）。
 * 有效区与 HOLD 一致：≥3.6V 采信；近满另有 REST 地板。
 */
#ifndef W20_BAT_SOC_ALIGN_VBAT_REF
#define W20_BAT_SOC_ALIGN_VBAT_REF 1
#endif
/** |ibat|＜此值才算静载候选（mA）；快充/快放下 OCV 无参考性。 */
#ifndef W20_BAT_SOC_ALIGN_QUIET_IBAT_MA
#define W20_BAT_SOC_ALIGN_QUIET_IBAT_MA 250
#endif
/**
 * IP 离线无 ibat 时：仍允许静载 VBAT 参考（假定无大电流）。
 * 关闭则恢复旧行为（无 ibat 永不 quiet → 拔电 NACK 后无法用 V 校准）。
 */
#ifndef W20_BAT_SOC_ALIGN_QUIET_NO_IBAT
#define W20_BAT_SOC_ALIGN_QUIET_NO_IBAT 1
#endif
/** 连续静载满此时长才采信 VBAT（ms）。 */
#ifndef W20_BAT_SOC_ALIGN_QUIET_HOLD_MS
#define W20_BAT_SOC_ALIGN_QUIET_HOLD_MS 4000u
#endif
/** 离开大电流后再等此时长，电压回弹后再信 VBAT（ms）。 */
#ifndef W20_BAT_SOC_ALIGN_HEAVY_SETTLE_MS
#define W20_BAT_SOC_ALIGN_HEAVY_SETTLE_MS 15000u
#endif
/** |UI−Vref|≥ 此百分点才纠（小误差不碰，避免抖）。 */
#ifndef W20_BAT_SOC_ALIGN_VBAT_GAP_MIN
#define W20_BAT_SOC_ALIGN_VBAT_GAP_MIN 8
#endif
#ifndef W20_BAT_SOC_ALIGN_VBAT_LEARN_MS
#define W20_BAT_SOC_ALIGN_VBAT_LEARN_MS 3000u
#endif
#ifndef W20_BAT_SOC_ALIGN_VBAT_EMA_DIV
#define W20_BAT_SOC_ALIGN_VBAT_EMA_DIV 16
#endif
#ifndef W20_BAT_SOC_ALIGN_VBAT_MAX_STEP
#define W20_BAT_SOC_ALIGN_VBAT_MAX_STEP 1
#endif
/**
 * 静载纠偏且 VBAT≥HOLD_BIAS_MV（默认≥3.6V）时：bias 可用此上限（≥BIAS_MAX），
 * 以便从 CW 拉伸偏低拉到 LUT / 近满地板。
 */
#ifndef W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX
#define W20_BAT_SOC_ALIGN_VBAT_BIAS_MAX 36
#endif
/**
 * 开机/拔电后：静载确认 VBAT 时一次性把 bias（及已显示的 UI）对齐到 LUT，
 * 避免「没充电却慢慢涨」的观感。关闭则仅保留慢纠。
 */
#ifndef W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP
#define W20_BAT_SOC_ALIGN_VBAT_BOOT_SNAP 1
#endif
/** CW 拉伸% ≤ 此值视为见底：不学正 bias，且 apply 时正 bias 贡献为 0。 */
#ifndef W20_BAT_SOC_ALIGN_EMPTY_CW_PCT
#define W20_BAT_SOC_ALIGN_EMPTY_CW_PCT 0
#endif
/** VBAT ≤ 此值（mV）时清掉正 bias（防 CW 未到 0 但电压已空仍被 bias 抬着）。 */
#ifndef W20_BAT_SOC_ALIGN_EMPTY_MV
#define W20_BAT_SOC_ALIGN_EMPTY_MV 3100
#endif
#ifndef W20_BAT_SOC_ALIGN_NVS
#define W20_BAT_SOC_ALIGN_NVS 0
#endif
/** 旧 lut_bias EMA 分母（已不用，保留宏以免外部覆盖编译失败）。 */
#ifndef W20_BAT_LUT_BIAS_EMA_DIV
#define W20_BAT_LUT_BIAS_EMA_DIV 8
#endif
#ifndef W20_BAT_LUT_BIAS_MAX_X100
#define W20_BAT_LUT_BIAS_MAX_X100 4000
#endif
/** 旧 CW/LUT 分歧阈值（已不用）。 */
#ifndef W20_BAT_CW_LUT_DISAGREE_PCT
#define W20_BAT_CW_LUT_DISAGREE_PCT 10
#endif
/**
 * 低电自动关机（进 SCREEN9：img_start +「Low battery off」，非软睡）：
 * - 仅看电压：稳定 VBAT ≤ FAILSAFE_MV(3.0V) 持续 hold（默认 10s）
 * - V≤3.0V 时 read_percent 已强制 UI=0；不再用「仅 UI%≤1」触发（防假低电量误关）
 * - |ibat|≥ MAX_IBAT 视为大电流压降，不计时；充电/VBUS 在时不触发
 */
#ifndef W20_BAT_AUTO_SHUTDOWN
#define W20_BAT_AUTO_SHUTDOWN 1
#endif
/** 电压到关机点时强制 UI% 对齐用（非自动关机触发条件）。 */
#ifndef W20_BAT_SHUTDOWN_PCT
#define W20_BAT_SHUTDOWN_PCT 1
#endif
#ifndef W20_BAT_SHUTDOWN_FAILSAFE_MV
#define W20_BAT_SHUTDOWN_FAILSAFE_MV 3000
#endif
#ifndef W20_BAT_SHUTDOWN_FAILSAFE_HOLD_MS
#define W20_BAT_SHUTDOWN_FAILSAFE_HOLD_MS 30000u
#endif
/** |ibat|≥该值视为大电流压降，不计时（避免误关）。无电流读数时仍允许计时。 */
#ifndef W20_BAT_SHUTDOWN_FAILSAFE_MAX_IBAT_MA
#define W20_BAT_SHUTDOWN_FAILSAFE_MAX_IBAT_MA 500
#endif
/** 兼容旧名：电压 hold 与 FAILSAFE_HOLD 相同。 */
#ifndef W20_BAT_SHUTDOWN_PCT_HOLD_MS
#define W20_BAT_SHUTDOWN_PCT_HOLD_MS W20_BAT_SHUTDOWN_FAILSAFE_HOLD_MS
#endif
/**
 * 棕断弱电锁存（打断 BROWNOUT 闪屏环）：
 * - 棕断复位且 last_alive 电压偏低 → 记录 latch，开机走低电关机页 + deep sleep
 * - 锁存期间每次启动都进低电路径，直到充电/VBUS 或电压明显回升后清除
 * - 学到的电压 + MARGIN 作为 latch_mv（日志/可选运行期参考）；关机判定以 locked 为主
 */
#ifndef W20_BAT_BOD_LATCH
#define W20_BAT_BOD_LATCH 1
#endif
/** 仅当 last_alive VBAT ≤ 该值才学习（防无线高电棕断误学）。 */
#ifndef W20_BAT_BOD_LEARN_MAX_MV
#define W20_BAT_BOD_LEARN_MAX_MV 3550
#endif
/** latch_mv = learned + MARGIN，并封顶 LEARN_MAX+MARGIN。 */
#ifndef W20_BAT_BOD_LATCH_MARGIN_MV
#define W20_BAT_BOD_LATCH_MARGIN_MV 80
#endif
/** IP 不可用时：当前 VBAT ≥ 该值视为已充电/恢复，清除锁存。 */
#ifndef W20_BAT_BOD_CLEAR_MIN_MV
#define W20_BAT_BOD_CLEAR_MIN_MV 3600
#endif
/**
 * GPIO10 KEY 持续为低达到该时长 → 判定 IP6802 进入电池路径（IP5356 可能休眠）。
 * 仅置 key_sleep；UI% 仍固定 CW。用于滤掉拔电后约 2s 短脉冲。
 */
#ifndef W20_IP5356_KEY_FORCE_CW_MS
#define W20_IP5356_KEY_FORCE_CW_MS 5000u
#endif
/**
 * 开机 I2C 失败后的运行期重探限频：两次 begin 最短间隔。
 * 仅由 GPIO10 KEY 边沿/周期触发，Battery/Test 读路径禁止 begin。
 */
#ifndef W20_IP5356_REPROBE_MIN_MS
#define W20_IP5356_REPROBE_MIN_MS 5000u
#endif
/**
 * KEY 保持高且仍 SKIPPED 时的兜底重探间隔（轻探失败时的备份；主路径见 BUS_RECOVER_MS）。
 */
#ifndef W20_IP5356_REPROBE_PERIODIC_MS
#define W20_IP5356_REPROBE_PERIODIC_MS 8000u
#endif
/**
 * IP5356 硬件掉电时 I2C NACK 为预期：连续读失败达此次数 → 标 SKIPPED，停止轮询（消 Wire Error 刷屏）。
 * 插电后由 BUS_RECOVER_MS 轻探 / KEY 周期重探恢复。
 */
#ifndef W20_IP5356_BUS_FAIL_STREAK
#define W20_IP5356_BUS_FAIL_STREAK 3u
#endif
/** SKIPPED（开机未找到或运行掉电）后轻量 ACK 探测间隔；插电应在此量级内恢复充电态。 */
#ifndef W20_IP5356_BUS_RECOVER_MS
#define W20_IP5356_BUS_RECOVER_MS 1500u
#endif
/** GPIO10 KEY 心跳间隔（需 W20_LOG_BAT）。 */
#ifndef W20_IP5356_KEY_HB_MS
#define W20_IP5356_KEY_HB_MS 5000u
#endif
/**
 * 充放标定 CSV 行间隔（需 W20_LOG_BAT）。抓串口保存为 .csv。
 */
#ifndef W20_BAT_CAL_CSV_MS
#define W20_BAT_CAL_CSV_MS 5000u
#endif
#define USE_SD_CARD          0   // SD not wired in this HW mapping
#define USE_PCM5102A_AUDIO   0
// Control whether GPS and LoRa initialization code runs in `factory.ino`.
#if defined(USE_W20_DECKPRO_SHELL)
/** 1=菜单 LVGL 刷墨在独立 FreeRTOS 任务（shell_loop 不阻塞 ~1.3s）；0=同步刷墨（旧行为）。 */
#ifndef W20_EPD_ASYNC_FLUSH
#define W20_EPD_ASYNC_FLUSH 1
#endif
#define USE_GPS              1
#else
#define USE_GPS              0
#endif
// Control whether touch panel initialization runs. Set to 0 to disable
// touch.init() and related callbacks if touch hardware or wiring is not present.
#define USE_TOUCH            1
// Enable LoRa support (module wired on separate SPI bus). Keep at 1 to build LoRa code.
#define USE_LORA             1
// Enable A7682E modem support (default disabled - set to 1 to enable)
#define USE_A7682E           0
// BQ/BHI feature toggles
// -------------------------------------------------

// I2C bus: shared (touch 0x58, IP5356 0x75, CW2217 0x64). IP5356 uses SCL/SDA only (LED1/LED2 mux), not GPIO38.
#define BOARD_I2C_SDA  8   // ESP_SDA -> GPIO8
#define BOARD_I2C_SCL  7   // ESP_SCL -> GPIO7

// I2S (not used by this board by default)
#define BOARD_I2S_BCLK -1
#define BOARD_I2S_DOUT -1
#define BOARD_I2S_LRC  -1

// Keys (GPIO-based)
// Three keys: RST_KEY (ESP_EN, not a GPIO), BOOT_KEY (GPIO0), LED_KEY (GPIO48)
#define PIN_KEY_BOOT   0
#define PIN_KEY_LED    48
// RST_KEY is ESP_EN; no GPIO macro (board reset)

// Legacy keypad (TCA8418) – disabled
#define BOARD_KEYBOARD_SCL BOARD_I2C_SCL
#define BOARD_KEYBOARD_SDA BOARD_I2C_SDA
#define BOARD_KEYBOARD_INT -1
// This board has a key named "LED_KEY" on GPIO48 (input), not an LED output.
// There is no dedicated "keyboard LED" GPIO; set to -1 and control LED via IP6802 when supported.
#define BOARD_KEYBOARD_LED -1

// Touch panel (FT6X36 family on system I2C)
#define BOARD_TOUCH_SCL BOARD_I2C_SCL
#define BOARD_TOUCH_SDA BOARD_I2C_SDA
#define BOARD_TOUCH_INT 18
#define BOARD_TOUCH_RST 17

// LTR-553ALS-WA (optional)
#define BOARD_ALS_SCL BOARD_I2C_SCL
#define BOARD_ALS_SDA BOARD_I2C_SDA
#define BOARD_ALS_INT -1

// Gyroscope (optional)
#define BOARD_GYROSCOPDE_SCL BOARD_I2C_SCL
#define BOARD_GYROSCOPDE_SDA BOARD_I2C_SDA
#define BOARD_GYROSCOPDE_INT -1
#define BOARD_GYROSCOPDE_RST -1

// SPI buses
// NOTE: Original firmware assumed a single shared SPI for LoRa/SD/EPD.
// This board wires LoRa and EPD on different pins. We will:
// - Keep the default SPI (SPI) mapped to the EPD pins for display
// - Handle LoRa on its own SPI pins via RadioLib (code change planned)
#define BOARD_SPI_SCK  46   // EPD SCLK
#define BOARD_SPI_MOSI 47   // EPD SDI (MOSI)
#define BOARD_SPI_MISO -1   // EPD does not use MISO

// Display (DEPG0310BxU253 / UC8253，与 factory GxEPD2 类名一致)
#define LCD_HOR_SIZE 240
#define LCD_VER_SIZE 320
#define DISP_BUF_SIZE (LCD_HOR_SIZE * LCD_VER_SIZE)

#define BOARD_EPD_SCK  BOARD_SPI_SCK
#define BOARD_EPD_MOSI BOARD_SPI_MOSI
#define BOARD_EPD_DC   42
#define BOARD_EPD_CS   45
#define BOARD_EPD_BUSY 40
#define BOARD_EPD_RST  41

#ifndef PIN_EINK_RES
#define PIN_EINK_RES   BOARD_EPD_RST
#define PIN_EINK_CS    BOARD_EPD_CS
#define PIN_EINK_DC    BOARD_EPD_DC
#define PIN_EINK_BUSY  BOARD_EPD_BUSY
#define PIN_EINK_SCLK  BOARD_EPD_SCK
#define PIN_EINK_MOSI  BOARD_EPD_MOSI
#endif

// SD card (not used on this board)
#define BOARD_SD_CS   -1
#define BOARD_SD_SCK  -1
#define BOARD_SD_MOSI -1
#define BOARD_SD_MISO -1

// LoRa LR2021 — same map as variants/.../variant.h
// MISO=11 MOSI=13 SCK=12 NSS=1 NRST=14 BUSY=4 DIO1=5 EN=39(ME6231)
#define BOARD_LORA_SCK   12
#define BOARD_LORA_MOSI  13
#define BOARD_LORA_MISO  11
#define BOARD_LORA_CS    1   // NSS (shared with GPS reset pad, GPS reset disabled)
#define BOARD_LORA_BUSY  4
#define BOARD_LORA_RST   14  // NRST
#define BOARD_LORA_INT   5   // DIO1 / chip DIO9

// GPS (Quectel L76K). WAKE=GPIO2; EN=GPIO6 → ME6231C33M5G GPS3V3 (HIGH=on).
#define BOARD_GPS_RXD   44
#define BOARD_GPS_TXD   43
#define BOARD_GPS_PPS   -1
#define BOARD_GPS_WAKE  2   // GPS wake-up
// GPS reset disabled due to conflict with LoRa NSS on GPIO1
#define BOARD_GPS_RESET -1

// A7682E Modem (not present on this board)
#define BOARD_A7682E_RI     -1
#define BOARD_A7682E_ITR    -1
#define BOARD_A7682E_RST    -1
#define BOARD_A7682E_RXD    -1
#define BOARD_A7682E_TXD    -1
#define BOARD_A7682E_PWRKEY -1

// Boot pin
#define BOARD_BOOT_PIN  PIN_KEY_BOOT

// Motor pin (not present)
#define BOARD_MOTOR_PIN -1

// Power enable pins (independent ME6231 LDOs, HIGH=on)
#define BOARD_GPS_EN    6   // ME6231C33M5G EN → GPS3V3
#define BOARD_1V8_EN   -1
#define BOARD_6609_EN  -1   // legacy 7682 power enable
#define BOARD_LORA_EN  39   // ME6231C33M5G EN → LORA3V3

// Charger / Battery
// IP5356M: battery/via I2C @0x75 + optional INT (e.g. GPIO9). It does NOT use GPIO38.
//
// 照明灯（w20 / GPIO38）：经 MOS 驱动。**默认关灯 MCU 输出 LOW**；开灯时输出 HIGH（按板上 MOS 接法）。
#define PIN_LIGHTING_LAMP  38
/** 照明灯亮时 GPIO 电平：本板为 HIGH=亮，LOW=灭。 */
#define LIGHTING_LAMP_ON_LEVEL   HIGH
#define LIGHTING_LAMP_OFF_LEVEL  LOW

/** 可选：独立电池分压接到 ADC 的引脚（与 GPIO38 照明灯无关）。未使用填 -1。仅当 USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356=1 时参与电量估算。 */
#ifndef PIN_VBAT_ADC
#define PIN_VBAT_ADC (-1)
#endif
#define PIN_IP5356_INT   9  // IP5356 INT
// GPIO10 — 检测 IP5356 pin21 KEY（网名 KEY_IP）。内部上拉，按下为低。
// 默认 INPUT。0x75 不在且 BOOT 短按时短暂 OUTPUT 打唤醒脉冲，随后回到 INPUT。
#define PIN_IP5356_KEY  10
#define IP5356_KEY_ACTIVE_LEVEL  LOW

/**
 * w20-mb：无 MCU 引脚去拉 CHIP_EN/ESP_EN；RST 按键直连 **EN**，仅硬件复位重启，固件不可模拟「掉电关机」。
 * 若其它改版原理图增加「GPIO→MOS→拉低 EN」再改为实际 GPIO；本板保持 -1。
 *
 * Shutdown（peri_power.cpp / w20_power_shutdown.cpp）：
 *   hold 低 — LORA_EN(39), GPS_EN(6), GPS_WAKE(2), LIGHTING_LAMP(38), RGB(3/21)
 *   EPD 常供 3.3V — hibernate 后安全 OUTPUT（勿 gpio_reset）
 *   LoRa/GPS — sleep/待机 → park 高阻 → EN 断电
 *   触摸/I2C/按键 — TOUCH_RST 高、TOUCH_INT/I2C INPUT、IP5356 KEY(10)/0/48 INPUT
 */
#ifndef PIN_ESP_EN_KILL
#define PIN_ESP_EN_KILL (-1)
#endif
/** 仅当 PIN_ESP_EN_KILL>=0 且原理图定义了有效关机电平时使用。 */
#ifndef PIN_ESP_EN_KILL_ACTIVE_LEVEL
#define PIN_ESP_EN_KILL_ACTIVE_LEVEL 1
#endif

// RGB0603 LED control
// R+ -> GPIO3, B+ -> GPIO21, G+ -> IO0_LED (not software-controlled)
#define PIN_RGB_R        3
#define PIN_RGB_B        21
// Set active level for direct GPIO channels
#define RGB_R_ACTIVE_LEVEL HIGH
#define RGB_B_ACTIVE_LEVEL HIGH

// Linear SOC from BAT voltage (single Li-ion): 3.0V→0% … 4.3V→99%；UI 100% 只认 CW≥FULL_ENTER（拔电）
#define IP5356_SOC_EMPTY_MV  3000
#define IP5356_SOC_FULL_MV   4300
/** 电压映射上限；与 IP5356_SOC_FULL_MV 对齐，100 只由拔电后 CW≥FULL_ENTER 给出。 */
#ifndef W20_BAT_SOC_VOLTAGE_CAP_PCT
#define W20_BAT_SOC_VOLTAGE_CAP_PCT 99
#endif
/** 电芯标称容量 (mAh)，用于 Cap Remain/Full 按 SOC% 估算（非库仑计实测）。 */
#define IP5356_BATTERY_DESIGN_MAH  10000

/** NTC：IP5356 恒流测 NTC 脚，10K@25°C，B=3380；默认 80µA（0xF6=00/01 时与实测 615mV 一致）。 */
#define IP5356_NTC_R0_OHM           10000
#define IP5356_NTC_B_VALUE          3380
#define IP5356_NTC_DEFAULT_I_UA     80
/** IP5356 配置寄存器组 I2C 7-bit 地址（8bit 写 0xE8），如 0xF6 NTC 电流源。 */
#define BOARD_I2C_ADDR_IP5356_CFG   0x74

// If 1 and PIN_BAT_ADC>=0: when IP5356 missing, estimate SOC from MCU ADC. Off when no divider wired (IP5356-only design).
#define USE_BAT_ADC_FALLBACK_WHEN_NO_IP5356  0
// VBAT_mV ≈ analogReadMilliVolts(PIN_BAT_ADC) * BAT_ADC_VBAT_NUM / BAT_ADC_VBAT_DEN
// 例：100k+100k 分压到 ADC 则 VBAT = 2 * Vpin → NUM=2 DEN=1
#define BAT_ADC_VBAT_NUM  2
#define BAT_ADC_VBAT_DEN  1

// Mic (unused)
#define BOARD_MIC_DATA        -1
#define BOARD_MIC_CLOCK       -1
// -------------------------------------------------


