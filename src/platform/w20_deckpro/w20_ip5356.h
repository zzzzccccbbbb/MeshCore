#pragma once

#include <Arduino.h>
#include <IP5356.h>

#if defined(W20_MESHNOLOGY)

extern IP5356 g_ip5356;

/**
 * 探测结果：
 * - boot 尝试一次；失败记 SKIPPED
 * - 运行期仅由 GPIO10 KEY 边沿/慢周期经 try_runtime_reprobe 再 begin
 * - Battery/Test 等读路径禁止 begin（ensure_ready 为空）
 */
enum w20_ip5356_probe_state_t {
    W20_IP5356_PROBE_NOT_TRIED = 0,
    W20_IP5356_PROBE_OK,
    W20_IP5356_PROBE_SKIPPED,
};

/** 单次 I2C probe @0x75（开机调用；勿在 ensure/UI 读路径反复调用）。 */
bool w20_ip5356_boot_init(void);

/** 强制重置状态后再 begin。已 OK 则直接返回 true。 */
bool w20_ip5356_reprobe(void);

/**
 * 运行期限频重探：未 OK 时才 begin，受 W20_IP5356_REPROBE_MIN_MS 约束。
 * @return true=当前已 present（含本次成功）；false=仍不可用或处于冷却中。
 */
bool w20_ip5356_try_runtime_reprobe(const char *reason);

w20_ip5356_probe_state_t w20_ip5356_probe_state(void);

/** 故意为空：禁止 Battery/Test 页因读寄存器反复 begin。 */
void w20_ip5356_ensure_ready(void);

bool w20_ip5356_is_present(void);
/** 最近一次寄存器读是否成功（供 KEY hb 等调试；与 is_present 缓存态可能不同）。 */
bool w20_ip5356_last_read_ok(void);
int w20_ip5356_read_voltage_mv(void);
int w20_ip5356_read_percent(void);
/** 0x7B 寄存器读成功返回 true，*percent 为芯片 SOC；否则为电压线性估算。 */
bool w20_ip5356_soc_from_register(int *percent);
bool w20_ip5356_read_status(IP5356::Status &st);
int w20_ip5356_read_input_current_ma(void);
int w20_ip5356_read_bat_current_ma(void);
int w20_ip5356_read_vin_mv(void);
int w20_ip5356_read_vbus_mv(void);
int w20_ip5356_read_ntc_mv(void);
/** 有效返回摄氏度（四舍五入）；无效返回 INT16_MIN。 */
int w20_ip5356_read_ntc_temp_c(void);

#endif /* W20_MESHNOLOGY */
