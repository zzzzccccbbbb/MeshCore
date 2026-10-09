#pragma once

#include <stdbool.h>

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "input/InputBroker.h"

#ifndef W20_DECK_SLEEP_WIFI_OFF
#define W20_DECK_SLEEP_WIFI_OFF 1
#endif

/** Sleep 进屏：1=powerOff 保画面（略增漏电）；0=hibernate 0x07 最低功耗但易渐暗。 */
#ifndef W20_DECK_SLEEP_EPD_HOLD_IMAGE
#define W20_DECK_SLEEP_EPD_HOLD_IMAGE 1
#endif

/** 主菜单（page_curr==0，含 WiFi Config 等 Deck 子页）：不因 screen_on_secs 自动 DARK/关屏。 */
bool w20_deck_skip_auto_dark_screen_off(void);

/** Deck 统一软睡眠（Mesh 超时 / 菜单 Sleep 子项）。 */
bool w20_deck_sleep_active(void);

/** 屏已关后调用：外设 off、禁触控/手势；不重复 setOn(false)。 */
void w20_deck_sleep_enter(void);

/** Boot 等唤醒：恢复触控；不自动开 WiFi。 */
void w20_deck_sleep_wake(void);

/** 菜单 Sleep：请求 PowerFSM 立即进入 DARK（随后 setOn(false) → enter）。 */
void w20_deck_sleep_request_dark(void);

/** PowerFSM DARK：菜单 Sleep 路径不打开 BLE（避免 darkEnter 先 setup 再 deinit）。 */
bool w20_deck_sleep_suppress_ble_in_dark(void);

void w20_deck_periph_sleep_off(void);
void w20_deck_periph_sleep_on(void);

/** shell_loop 内 Sleep 期间轮询 Boot 下降沿唤醒。 */
void w20_deck_sleep_poll_boot(void);

/** Boot 唤醒后短暂屏蔽 USER_PRESS，避免 Meshtastic showNextFrame 跳到 messages。 */
bool w20_deck_sleep_should_suppress_boot_nav(input_broker_event ev);

/** Boot 唤醒开始时调用，屏蔽随后 Meshtastic USER_PRESS 切屏。 */
void w20_deck_sleep_arm_boot_nav_suppress(void);

/** 为 true 时 w20SyncSetOn(true) 不触发 runASAP（由 forceDisplay(true) 单次全刷）。 */
bool w20_deck_sleep_defer_screen_run(void);
void w20_deck_sleep_set_defer_screen_run(bool defer);

/** 兼容旧名 */
bool w20_deck_board_sleep_active(void);
void w20_deck_board_sleep_arm(void);
void w20_deck_board_sleep_wake(void);

#endif
