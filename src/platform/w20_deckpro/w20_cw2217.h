#pragma once

#include <Arduino.h>

#if defined(W20_MESHNOLOGY)

/** CW2217EAAD-DS fuel gauge @ I2C 0x64 (write 0xC8 / read 0xC9). INT_N is a test point only. */
bool w20_cw2217_boot_init(void);
bool w20_cw2217_is_present(void);
/**
 * 运行期恢复：present 但 VCELL/SOC 变 0（多为未 ACTIVE）时限频调用。
 * 成功后可读到有效电压；失败保持原状。
 */
bool w20_cw2217_try_recover(void);
/** Integer SOC 0–100, or -1 if unavailable. */
int w20_cw2217_read_percent(void);
/**
 * SOC ×100（如 8330 = 83.30%），含 0x05 小数字节；失败返回 -1。
 * 便于与 IP5356 对表时看细粒度漂移。
 */
int w20_cw2217_read_percent_x100(void);
/** Battery voltage in mV, or -1 if unavailable. */
int w20_cw2217_read_voltage_mv(void);
/** SOH 0–100, or -1 if unavailable. */
int w20_cw2217_read_soh_percent(void);

#endif
