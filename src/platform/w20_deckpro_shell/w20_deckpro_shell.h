#pragma once

#include <Arduino.h>
#include <TouchDrvCST3530.hpp>

/** 与 variant / shell 共用，供 Meshtastic 页直接读滑动手势。 */
extern TouchDrvCST3530 touch;

/** True while Meshtastic Screen thread should refresh the E-Ink (second main-menu page). */
bool w20_shell_mesh_screen_allowed(void);

/** Main menu page index from Deck Pro UI: 0 = home grid, 1 = second page (Meshtastic UI). */
void w20_shell_on_menu_page(int page_index);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
/** 进 Mesh 切换期间为 true：跳过 runOnce 里 FIXED 时的 forceDisplay（on_menu_page 已 forceDisplay(true)）。 */
bool w20_shell_skip_mesh_idle_force(void);
/** 唤醒/切页单次全刷期间置 true，避免与 forceDisplay(true) 叠刷。 */
void w20_shell_set_skip_mesh_idle_force(bool skip);
/** 睡眠唤醒后：保持 skip_mesh_idle_force 若干毫秒，避免 FIXED/顶栏叠刷。 */
void w20_shell_arm_wake_epd_skip_ms(uint32_t ms);
/** Meshtastic 切页 EPD 抑制：准备 UI 期间 false，仅允许一次 flush。 */
bool w20_shell_mesh_epd_may_update(void);
/** 绕过 1s 限流，强制一次 EPD 全刷（进 Mesh / 睡眠唤醒）。 */
void w20_shell_mesh_epd_flush_once(void);
/** overlay 菜单输入期间：仅允许 flush_once 刷墨，避免与 runOnce 叠刷。 */
void w20_shell_mesh_overlay_epd_begin(void);
void w20_shell_mesh_overlay_epd_end(void);
/** 主菜单后台预构建 Mesh UI（不刷 EPD）期间为 true。 */
bool w20_shell_mesh_ui_prewarm_active(void);
/** 主菜单空闲时预构建 Meshtastic 帧列表与首帧 buffer（可重复调用，仅首次生效）。 */
void w20_shell_prewarm_mesh_ui(void);

/**
 * 刷新后保持画面：1=仅 powerOff（不进 0x07 深睡），减轻数秒后白底变暗/黑印；0=hibernate 省电。
 * 可在 platformio.ini 用 -D W20_DECKPRO_EPD_HOLD_IMAGE=0 恢复旧行为。
 */
#ifndef W20_DECKPRO_EPD_HOLD_IMAGE
#define W20_DECKPRO_EPD_HOLD_IMAGE 1
#endif
void w20_deckpro_epd_after_refresh(void);
/** 与 Meshtastic forceDisplay / 异步刷墨任务互斥，调用方成对使用。 */
void w20_deckpro_epd_lock(void);
void w20_deckpro_epd_unlock(void);
/** 关机/休眠前等待进行中的刷墨结束。 */
void w20_deckpro_epd_drain(void);
#endif

void w20_deckpro_shell_init(void);
void w20_deckpro_shell_loop(void);
/** Mesh→菜单：先跑完 LVGL 再 FULL 刷墨水，避免 stale decodebuffer 白屏。 */
void w20_deckpro_present_menu_on_epd(void);

/**
 * 极早开机 splash：仅 EPD SPI + FULL 刷 img_start，在 Meshtastic 长 setup 之前调用。
 * 与后续 shell 共用同一 GxEPD2 实例；UI 侧跳过 SCREEN12 避免重复。
 */
void w20_deckpro_early_splash(void);
/** Sleep 进睡前：FULL 刷 img_start + 底部 Sleep 文案，再 powerOff 保画面。 */
void w20_deckpro_sleep_prompt_flush(void);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#endif

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
/** `w20_deckpro_shell_init()` 完成后为 true；EInkDisplay 与 shell 共用同一 GxEPD2 实例。 */
bool w20_deckpro_shell_epd_reuse_ready(void);
void *w20_deckpro_shell_epd_gx_bw_ptr(void);
#endif
