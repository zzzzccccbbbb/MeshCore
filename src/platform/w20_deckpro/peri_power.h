#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Deep sleep：关外设后由 BOOT（GPIO0）低电平唤醒。 */
void power_enter_sleep(void);

/**
 * 关机：若 PIN_ESP_EN_KILL>=0 则驱动该脚（改版硬件）；否则无软件唤醒源 deep sleep。
 * w20 仅 RST→EN：恢复靠按 RST 或重新上电。
 */
void power_shutdown_execute(void);

/** W20 菜单 Sleep：EPD hibernate + 触控 sleep；不断 LoRa/GPS 电源 EN。 */
void power_deck_sleep_enter_periph(void);
void power_deck_sleep_exit_periph(void);

#ifdef __cplusplus
}
#endif
