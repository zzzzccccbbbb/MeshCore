/************************************************************************
 * FilePath     : ui_base.h
 * Author       : GX.Duan
 * LastEditors  : ShallowGreen123 2608653986@qq.com
 * Copyright (c): 2022 by GX.Duan, All Rights Reserved.
 * Github       : https://github.com/ShallowGreen123/lvgl_examples.git
 ************************************************************************/
#ifndef __UI_DECKPRO_H__
#define __UI_DECKPRO_H__

#ifdef __cplusplus
extern "C" {
#endif
/*********************************************************************************
 *                                  INCLUDES
 * *******************************************************************************/
#include "lvgl.h"
#include "ui_scr_mrg.h"

/*********************************************************************************
 *                                   DEFINES
 * *******************************************************************************/
// #ifdef __has_include
//     #if __has_include("lv_i18n.h")
//         #ifndef LV_i18N_INCLUDE_SIMPLE
//             #define LV_i18N_INCLUDE_SIMPLE
//         #endif
//     #endif
// #endif

// #if defined(LV_i18N_INCLUDE_SIMPLE)
//     #include "lv_i18n.h"
// #else
//     #define _(text) (text)
//     #define _p(text, num) (text, num)
// #endif
/*********************************************************************************
 *                                   MACROS
 * *******************************************************************************/
#define DECKPRO_COLOR_BG        lv_color_white()
#define DECKPRO_COLOR_FG        lv_color_black()
#define UI_SLIDING_DISTANCE     30
/** 同向水平位移在此时间窗内累加（ms），应对 CST3530 多段 DOWN/UP */
#define UI_GESTURE_ACCUM_WINDOW_MS 400
#define UI_WIFI_SCAN_ITEM_MAX   13

/*********************************************************************************
 *                                  TYPEDEFS
 * *******************************************************************************/
enum {
    SCREEN0_ID = 0,
    SCREEN1_ID,
    SCREEN1_1_ID,
    SCREEN1_2_ID,
    SCREEN2_ID,
    SCREEN2_1_ID,
    SCREEN3_ID,
    SCREEN4_ID,
    SCREEN4_1_ID,
    SCREEN4_2_ID,
    SCREEN4_3_ID,
    SCREEN5_ID,
    SCREEN6_ID,
    SCREEN6_1_ID,
    SCREEN6_2_ID,
    SCREEN7_ID,
    SCREEN8_ID,
    SCREEN8_1_ID,
    SCREEN8_2_ID,
    SCREEN9_ID,
    SCREEN10_ID,
    SCREEN11_ID,   /* eBook TOC */
    SCREEN11_1_ID, /* eBook reader */
    SCREEN12_ID,
};

typedef void (*ui_indev_read_cb)(int);

enum {
    TASKBAR_ID_CHARGE,
    TASKBAR_ID_CHARGE_FINISH,
    TASKBAR_ID_BATTERY_PERCENT,
    TASKBAR_ID_WIFI,
    TASKBAR_ID_MAX,
};

struct menu_btn {
    uint16_t idx;
    const void *icon;
    const char *name;
    lv_coord_t pos_x;
    lv_coord_t pos_y; 
};

enum{
    UI_SETTING_TYPE_SW,
    UI_SETTING_TYPE_RGB,
    UI_SETTING_TYPE_SUB,
};

typedef struct _ui_setting
{
    const char *name;
    int type;
    void (*set_cb)(bool);
    bool (*get_cb)(void);
    void (*set_mode_cb)(int);
    int (*get_mode_cb)(void);
    int sub_id;
    lv_obj_t *obj;
    lv_obj_t *st;
} ui_setting_handle;

typedef struct _ui_test {
    const char *name;
    int peri_id;
    lv_obj_t *obj;
    lv_obj_t *st;
    bool (*cb)(int);
} ui_test_handle;

typedef struct _ui_a7682 {
    const char *name;
    lv_obj_t *obj;
    lv_obj_t *st;
    bool (*cb)(const char *at_cmd);
} ui_a7682_handle;

typedef struct _ui_pcm5102 {
    const char *name;
    lv_obj_t *obj;
    lv_obj_t *st;
    bool (*cb)(const char *at_cmd);
} ui_pcm5102_handle;


typedef struct {
    char name[16];
    int rssi;
}ui_wifi_scan_info_t;

/** 去重后的扫描项（完整 SSID，供配网 JSON 等使用）。 */
typedef struct {
    char ssid[33];
    int rssi;
} ui_wifi_scan_entry_t;
/*********************************************************************************
 *                              GLOBAL PROTOTYPES
 * *******************************************************************************/
void ui_deckpro_entry(void);
void ui_deckpro_pause_aux_timers(void);
void ui_deckpro_resume_aux_timers(void);
/** 主页滑动手势；Meshtastic 页也需调用（不依赖 lv_task_handler）。 */
void ui_deckpro_poll_gesture(void);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
/** Mesh→菜单 FULL 刷前：复位滚动/布局，避免首帧偏移。 */
void ui_deckpro_menu_prepare_for_epd(void);
/** 水平滑动手势进行中不向 LVGL 上报按下，避免拖动九宫格。 */
bool ui_deckpro_lvgl_touch_suppressed(void);
/** 离开主菜单时清零滑动手势状态。 */
void ui_deckpro_gesture_reset(void);
/** 菜单 Sleep：切到 Mesh 页 + Screen Paused，禁用触控；Boot 单击唤醒。 */
void ui_deckpro_enter_board_sleep(void);
/** 进子菜单前：取消待刷 PART，下次 LVGL 刷新走 FULL。 */
void ui_deckpro_epd_prepare_subscreen(void);
/** 导航切换期间禁止墨水 flush（抬手→push 之间）；prepare_subscreen 会清除。 */
void ui_deckpro_epd_nav_hold(bool hold);
/** 子菜单 FULL 刷未完成：true 时 LVGL 点击会被吞，应排队导航。 */
bool ui_deckpro_epd_nav_touch_locked(void);
/** EPD 正在刷墨（pending 或尚未 done）：关机页等可见后再倒计时。 */
bool ui_deckpro_epd_flush_busy(void);
/** 主菜单 taskbar 电量图标/百分比立即刷新（IP5356）。 */
void ui_deckpro_refresh_taskbar_battery(void);
/** Power/USB 变化时刷新当前 Deck 屏（主菜单 taskbar 或 Battery 页）；Mesh 页由 Screen 刷。 */
void ui_deckpro_on_power_status_changed(void);
/** shell_loop 轮询 VBUS 边沿，插拔 Type-C 后尽快 powerRefreshStatus。 */
void ui_deckpro_poll_power_ui(void);
/** 主菜单 taskbar 时钟（Meshtastic RTC / 时区）。 */
void ui_deckpro_refresh_taskbar_time(void);
/** 主菜单 Mesh 图标进入 Meshtastic 页。 */
void ui_deckpro_enter_mesh(void);
/** Meshtastic 首页左滑返回主菜单。 */
void ui_deckpro_exit_mesh(void);
/** @deprecated 使用 ui_deckpro_exit_mesh() */
void ui_deckpro_mesh_swipe_back(void);
/** 主菜单九宫格根屏：轮询 Boot 长按进入 Shutdown（SCREEN9）。 */
void ui_deckpro_poll_boot_shutdown(void);
/** 低电自动关机：进 SCREEN9（img_start + 底部 Low battery off），再 hold 后真关机。 */
void ui_deckpro_poll_low_batt_shutdown(void);
/** Deck 占屏时吞掉 Meshtastic 对 Boot 的短/长按/关机，避免刷出 Mesh「Shutting Down...」。 */
bool ui_deckpro_suppress_mesh_boot_key(int input_event);
/** Deck Shutdown（SCREEN9）正在显示：Mesh Power::shutdown 勿再刷屏。 */
bool ui_deckpro_shutdown_ui_active(void);
/** 按 get_cb/get_mode_cb 刷新 Setting 列表右侧 ON/OFF、RGB 文案（LED 键与 entry 同步用）。 */
void ui_deckpro_setting_refresh_labels(void);
/** 开关/RGB 已改硬件：停手约 400ms 后再刷标签并整屏，避免连点叠全刷。不在 Setting 页则忽略。 */
void ui_deckpro_setting_defer_refresh(void);
/** 取消尚未触发的 Setting 延时刷屏（离开页面或 entry 已立即刷新时）。 */
void ui_deckpro_setting_defer_cancel(void);
#endif

#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif /* __UI_EPD47H__ */
