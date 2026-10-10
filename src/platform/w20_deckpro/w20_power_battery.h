#pragma once

#if defined(W20_MESHNOLOGY)

/**
 * Register CW2217 (UI%) + IP5356 (charge/VBUS/current) as HasBatteryLevel.
 * 开机打印 [PWR] 复位原因 + RTC last_alive，用于区分棕断/整轨掉电/看门狗。
 */
bool w20PowerBatteryInit(void);

/** 运行期 IP5356 从 SKIPPED 变为 OK 后调用：补注册电量并刷新充电状态。 */
void w20_power_battery_on_ip5356_ready(void);

/**
 * 统一 SOC（UI 百分比）：
 * - 默认：CW 拉伸（FULL_RAW）+ IP 在线时学 bias，休眠保留 bias
 * - W20_BAT_UI_SRC_IP5356=1：改为直接跟 IP 0x7B（试跑）
 * - 插电封顶 99；对号亮时 100
 * 无效返回 -1。
 */
int w20_battery_read_percent(void);

/** 当前电池电压估计（mV）；无效返回 -1。 */
int w20_battery_read_vbat_mv(void);

/** 当前 UI SOC 数据源名："CW2217" / "IP5356" / "V_LUT" / "none"。 */
const char *w20_battery_ui_src_name(void);

/**
 * KEY 持续低电平达到阈值后置 true（IP 可能休眠）；KEY 回高后清 false。
 * 不影响 UI%；仅影响 CW 失败时 LUT 表选择。
 */
void w20_battery_set_key_sleep_mode(bool on);
bool w20_battery_key_sleep_mode(void);

/** 当前 IP→CW 校准偏置（百分点）；ALIGN=0 时恒为 0。 */
int w20_battery_soc_align_bias(void);

/** 周期调试/采数日志。由 shell_loop 调用。 */
void w20_battery_dual_soc_log_poll(void);

/**
 * 充满对号：IP Finish +（拉伸+bias 后）UI≥FINISH_ICON_MIN（IP 主源试跑时仅 Finish）。
 * 对号亮时 UI% 同步为 100。
 */
bool w20_battery_charge_complete_latched(void);

/**
 * 低电自动关机判定（主路径 0%、兜底低电压）。shell/UI 周期调用。
 * 一旦满足条件即锁存，直到复位。
 */
void w20_battery_auto_shutdown_poll(void);
bool w20_battery_auto_shutdown_pending(void);

#endif
