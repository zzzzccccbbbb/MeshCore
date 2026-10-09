#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

/**
 * W20 最低功耗关机：停 Meshtastic 射频/GPS/BLE，扩展外设断电与 GPIO hold，无唤醒 deep sleep。
 * 标准 w20-mb 仅 RST/上电可恢复（PIN_ESP_EN_KILL=-1）。
 */
void w20_power_shutdown_execute(void);

#endif
