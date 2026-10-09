
#include "ui_deckpro.h"
#include "src/assets.h"
#include "Arduino.h"
#include "stdio.h"
#include "ui_deckpro_port.h"
#include "factory.h"
#include "w20_dbg_log.h"
#include "peripheral.h"
#include "peri_power.h"
#include "ebook_catalog.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <stdint.h>

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "graphics/Screen.h"
#include "graphics/draw/NotificationRenderer.h"
#include "input/InputBroker.h"
#include "mesh/wifi/WiFiAPClient.h"
#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"
#include "platform/w20_deckpro_shell/w20_lora_arbiter.h"
#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro_shell/w20_ui_debug.h"
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#include "platform/w20_deckpro_shell/w20_meshcore_ui.h"
#include "platform/w20_deckpro/w20_wifi_ota.h"
#include "platform/w20_deckpro/w20_power_battery.h"
#include "power.h"
#include "main.h"
#include "ui_scr_mrg.h"
extern graphics::Screen *screen;
void ui_deckpro_enter_board_sleep(void);
#endif

#define SETTING_PAGE_MAX_ITEM 7
/** Setting：行间缝收紧、按键加高；CLICK_EXT 随 PAD_ROW 联动。 */
#define SETTING_ITEM_BTN_H    40
#define SETTING_ITEM_PAD_ROW  8
#define SETTING_ITEM_PAD_TOP  14
#define SETTING_ITEM_CLICK_EXT ((SETTING_ITEM_PAD_ROW + 1) / 2)
/** Test：沿用旧布局（行高 35、行间距 8），与 Setting 解耦。 */
#define TEST_ITEM_BTN_H       35
#define TEST_ITEM_PAD_ROW     8
#define TEST_ITEM_PAD_TOP     14
#define TEST_ITEM_CLICK_EXT   ((TEST_ITEM_PAD_ROW + 1) / 2)
#define GET_BUFF_LEN(a) sizeof(a)/sizeof(a[0])

#define FONT_BOLD_SIZE_14 &Font_Mono_Bold_14
#define FONT_BOLD_SIZE_15 &Font_Mono_Bold_15
#define FONT_BOLD_SIZE_16 &Font_Mono_Bold_16
#define FONT_BOLD_SIZE_17 &Font_Mono_Bold_17
#define FONT_BOLD_SIZE_18 &Font_Mono_Bold_18
#define FONT_BOLD_SIZE_19 &Font_Mono_Bold_19

#define FONT_BOLD_MONO_SIZE_14 &Font_Mono_Bold_14
#define FONT_BOLD_MONO_SIZE_15 &Font_Mono_Bold_15
#define FONT_BOLD_MONO_SIZE_16 &Font_Mono_Bold_16
#define FONT_BOLD_MONO_SIZE_17 &Font_Mono_Bold_17
#define FONT_BOLD_MONO_SIZE_18 &Font_Mono_Bold_18
#define FONT_BOLD_MONO_SIZE_19 &Font_Mono_Bold_19

#define GLOBAL_BUF_LEN 30
/** 主菜单 taskbar 时钟/电量合并刷新周期（秒）。 */
#define MENU_TASKBAR_REFRESH_SEC 60
static char global_buf[GLOBAL_BUF_LEN];

static lv_timer_t *touch_chk_timer = NULL;
static lv_timer_t *taskbar_update_timer = NULL;
static lv_obj_t *label_list[10] = {0};
uint16_t taskbar_statue[TASKBAR_ID_MAX] = {0};

//************************************[ Other fun ]******************************************
#if 1
static lv_obj_t *scr_back_btn_create(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_height(btn, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 3, 3);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, DECKPRO_COLOR_BG, LV_PART_MAIN);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    /* E-Ink：按下态与默认一致，避免返回前多刷一次 PART */
    lv_obj_set_style_bg_color(btn, DECKPRO_COLOR_BG, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
#endif
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    /* 勿过大：会与下方第一列表项重叠（WIFI Scan 曾被当成返回） */
    lv_obj_set_ext_click_area(btn, 6);

    lv_obj_t *label2 = lv_label_create(btn);
    lv_obj_align(label2, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_color(label2, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_label_set_text(label2, LV_SYMBOL_LEFT);

    lv_obj_t *label = lv_label_create(parent);
    lv_obj_align_to(label, label2, LV_ALIGN_OUT_RIGHT_MID, 5, -1);
    lv_obj_set_style_text_font(label, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_ext_click_area(label, 8);

    return label;
}

static const char *line_full_format(int max_c, const char *str1, const char *str2)
{
    if (!str1) {
        str1 = "";
    }
    if (!str2) {
        str2 = "";
    }
    if (max_c <= 1 || max_c >= GLOBAL_BUF_LEN) {
        max_c = GLOBAL_BUF_LEN - 1;
    }

    const int len1 = (int)strlen(str1);
    const int len2 = (int)strlen(str2);
    int j = 0;
    const int copy1 = len1 < max_c ? len1 : max_c;
    for (; j < copy1; j++) {
        global_buf[j] = str1[j];
    }
    const int pad_end = max_c - 1 - len2;
    for (; j < pad_end && j < max_c - 1; j++) {
        global_buf[j] = ' ';
    }
    for (int k = 0; k < len2 && j < max_c - 1; k++, j++) {
        global_buf[j] = str2[k];
    }
    global_buf[j] = '\0';
    /* 勿用 printf：走 newlib stdout/__swsetup_r；stdout 未就绪时会 LoadProhibited@0xc */
    return (const char *)global_buf;
}

#endif
//************************************[ screen 0 ]****************************************** menu
#if 1
#define MENU_BTN_NUM (sizeof(menu_btn_list) / sizeof(menu_btn_list[0]))

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
/** 右列 GPS/Battery/Shutdown：仅触控区相对图标偏移（显示位置不变） */
#define MENU_BTN_RIGHT_HIT_OFS_X 0
#define MENU_BTN_RIGHT_HIT_OFS_Y 0
#endif

static ui_indev_read_cb ui_get_gesture_dir = NULL;

static lv_obj_t *menu_screen1;
static lv_obj_t *menu_screen2;
static lv_obj_t *ui_Panel4;

static lv_obj_t * menu_taskbar = NULL;
static lv_obj_t * menu_taskbar_time = NULL;
static lv_obj_t * menu_taskbar_charge = NULL;
static lv_obj_t * menu_taskbar_battery = NULL;
static lv_obj_t * menu_taskbar_battery_percent = NULL;
static lv_obj_t * menu_taskbar_wifi = NULL;

static void menu_taskbar_refresh_all(void);

static int page_num = 0;
static int page_curr = 0;

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
/** push 后仅一次 FULL 刷标记，避免 entry 内 ui_disp_full_refr 与 prepare 叠刷导致原页闪一下。 */
static void scr_push_subscreen(int id)
{
    ui_deckpro_epd_nav_hold(true);
    scr_mgr_push(id, false);
    ui_deckpro_epd_prepare_subscreen();
}

/** WiFi 子菜单 push：抬手后仍 hold 墨水，直接刷子页，避免先 PART 父列表。 */
static void scr_push_wifi_subscreen(int id)
{
    ui_deckpro_pause_aux_timers();
    ui_deckpro_epd_nav_hold(true);
    factory_pause_flush_timer();
    scr_mgr_push(id, false);
    ui_deckpro_epd_prepare_subscreen();
}

void ui_deckpro_enter_mesh(void)
{
    if (w20_shell_mesh_screen_allowed()) {
        return;
    }
    w20_shell_prewarm_mesh_ui();
    ui_deckpro_gesture_reset();
    w20_shell_on_menu_page(1);
    W20_UI_LOG("mesh enter via menu");
}

void ui_deckpro_exit_mesh(void)
{
    if (!w20_shell_mesh_screen_allowed()) {
        return;
    }
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (screen != nullptr) {
        screen->w20CancelOverlayNavEpdFlush();
    }
#endif
    ui_deckpro_gesture_reset();
    w20_shell_on_menu_page(0);
    W20_UI_LOG("mesh exit -> deck menu");
}
#else
static void scr_push_subscreen(int id)
{
    scr_mgr_push(id, false);
}
#endif

static struct menu_btn menu_btn_list[] = 
{
    {SCREEN1_ID,  &img_lora,    "Lora",     23,     13},  // Page one
    {SCREEN2_ID,  &img_setting, "Setting",  95,     13},
    {SCREEN3_ID,  &img_GPS,     "GPS",      167,    13},
    {SCREEN4_ID,  &img_wifi,    "Wifi",     23,     101},
    {SCREEN7_ID,  &img_touch,   "Mesh",     95,     101},
    {SCREEN6_ID,  &img_batt,    "Battery",  167,    101},
    {SCREEN5_ID,  &img_test,    "Test",     23,     189},
    {SCREEN11_ID, &img_ebook,   "eBook",    95,     189},
    {SCREEN9_ID,  &img_lora,    "Shutdown", 167,    189},
    /* 2026-09-21: 删除原第 10 项 PCM5102（pos 与 Lora 重叠；page_num=0 后不再有第二页展示）。 */
};

static void menu_btn_event_cb(lv_event_t *e)
{
    struct menu_btn *tgr = (struct menu_btn *)e->user_data;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    lv_point_t pt = {0, 0};
    lv_indev_t *indev = lv_indev_get_act();
    if (indev != nullptr) {
        lv_indev_get_point(indev, &pt);
    }
    W20_UI_LOG("menu_btn click id=%d name=%s lv(%d,%d) page_curr=%d", tgr->idx, tgr->name, (int)pt.x, (int)pt.y,
               page_curr);
    if (tgr->idx == SCREEN7_ID) {
        ui_deckpro_enter_mesh();
        return;
    }
#endif
    scr_push_subscreen(tgr->idx);
}

static void menu_get_gesture_dir(int dir)
{
    /* 2026-09-21: 主菜单单页（page_num=0），左右滑不切第二页，直接吞掉。 */
    if (page_num <= 0) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        W20_UI_LOG("gesture_page IGNORE dir=%d (page_num=%d, single-page menu)", dir, page_num);
#endif
        return;
    }

    const int prev = page_curr;
    if (dir == LV_DIR_LEFT) {
        if (page_curr < page_num) {
            page_curr++;
        } else {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
            W20_UI_LOG("gesture_page LEFT blocked page_curr=%d page_num=%d", page_curr, page_num);
#endif
            return;
        }
    } else if (dir == LV_DIR_RIGHT) {
        if (page_curr > 0) {
            page_curr--;
        } else {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
            W20_UI_LOG("gesture_page RIGHT blocked page_curr=0");
#endif
            return;
        }
    }

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    W20_UI_LOG("gesture_page dir=%d page %d->%d", dir, prev, page_curr);
#endif

    if(page_curr == 1) {
        lv_obj_clear_flag(menu_screen2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(menu_screen1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(lv_obj_get_child(ui_Panel4, 0), lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(lv_obj_get_child(ui_Panel4, 1), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);

    } else if(page_curr == 0) {
        lv_obj_clear_flag(menu_screen1, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(menu_screen2, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(lv_obj_get_child(ui_Panel4, 0), lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(lv_obj_get_child(ui_Panel4, 1), lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        if (menu_screen1 != nullptr) {
            lv_obj_scroll_to(menu_screen1, 0, 0, LV_ANIM_OFF);
        }
        if (menu_screen2 != nullptr) {
            lv_obj_scroll_to(menu_screen2, 0, 0, LV_ANIM_OFF);
        }
#endif
    }
}

static void menu_btn_create(lv_obj_t *parent, struct menu_btn *info)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_width(btn, 50);
    lv_obj_set_height(btn, 50);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_OVERFLOW_VISIBLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(btn, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
#if !defined(W20_MESHNOLOGY) || !defined(USE_W20_DECKPRO_SHELL)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 3, LV_PART_MAIN | LV_STATE_PRESSED);
#endif

    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_obj_set_width(label, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(label, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(label, 0);
    lv_obj_set_y(label, 20);
    lv_obj_set_align(label, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_x(btn, info->pos_x);
    lv_obj_set_y(btn, info->pos_y);
    lv_obj_set_style_bg_img_src(btn, info->icon, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, (info->name));
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    const bool right_col_touch_ofs =
        (info->idx == SCREEN3_ID || info->idx == SCREEN6_ID || info->idx == SCREEN9_ID);
    if (right_col_touch_ofs) {
        /* 显示留在 pos_x/pos_y；透明按钮承载偏移后的命中区 */
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t *hit = lv_btn_create(parent);
        lv_obj_remove_style_all(hit);
        lv_obj_set_size(hit, 50, 50);
        lv_obj_set_pos(hit, info->pos_x + MENU_BTN_RIGHT_HIT_OFS_X, info->pos_y + MENU_BTN_RIGHT_HIT_OFS_Y);
        lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(hit, 0, LV_PART_MAIN);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hit, menu_btn_event_cb, LV_EVENT_CLICKED, (void *)info);
        lv_obj_set_ext_click_area(hit, 10);
    } else {
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void *)info);
    }
#else
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, (void *)info);
#endif
}

static void create0(lv_obj_t *parent) 
{
    int status_bar_height = 25;

    menu_taskbar = lv_obj_create(parent);
    lv_obj_set_size(menu_taskbar, LV_HOR_RES, status_bar_height);
    lv_obj_set_style_pad_all(menu_taskbar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_taskbar, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(menu_taskbar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(menu_taskbar, LV_OBJ_FLAG_SCROLLABLE);
    
    menu_taskbar_time = lv_label_create(menu_taskbar);
    lv_obj_set_style_border_width(menu_taskbar_time, 0, 0);
    lv_label_set_text(menu_taskbar_time, "12:00");
    lv_obj_set_style_text_font(menu_taskbar_time, &Font_Mono_Bold_14, LV_PART_MAIN);
    lv_obj_align(menu_taskbar_time, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *status_parent = lv_obj_create(menu_taskbar);
    lv_obj_set_size(status_parent, lv_pct(80)-2, status_bar_height-2);
    lv_obj_set_style_pad_all(status_parent, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(status_parent, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(status_parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_parent, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(status_parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(status_parent, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(status_parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(status_parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(status_parent, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(status_parent, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(status_parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(status_parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(status_parent, LV_ALIGN_RIGHT_MID, 0, 0);

    menu_taskbar_wifi = lv_label_create(status_parent);
    lv_label_set_text_fmt(menu_taskbar_wifi, "%s", LV_SYMBOL_WIFI);
    lv_obj_add_flag(menu_taskbar_wifi, LV_OBJ_FLAG_HIDDEN);

    menu_taskbar_charge = lv_label_create(status_parent);
    lv_label_set_text_fmt(menu_taskbar_charge, "%s", LV_SYMBOL_CHARGE);
    lv_obj_add_flag(menu_taskbar_charge, LV_OBJ_FLAG_HIDDEN);

    if(taskbar_statue[TASKBAR_ID_WIFI])
        lv_obj_clear_flag(menu_taskbar_wifi, LV_OBJ_FLAG_HIDDEN);

    if(taskbar_statue[TASKBAR_ID_CHARGE])
        lv_obj_clear_flag(menu_taskbar_charge, LV_OBJ_FLAG_HIDDEN);

    menu_taskbar_battery = lv_label_create(status_parent);
    lv_label_set_text(menu_taskbar_battery, LV_SYMBOL_BATTERY_EMPTY);

    menu_taskbar_battery_percent = lv_label_create(status_parent);
    lv_obj_set_style_text_font(menu_taskbar_battery_percent, &Font_Mono_Bold_14, LV_PART_MAIN);
    lv_label_set_text(menu_taskbar_battery_percent, "0");

    //
    /* 2026-09-21: 主菜单改为单页（9 个 menu_btn），不再左右滑切第二页。
     * 原 page_num = MENU_BTN_NUM / 9，page_num > 0 时显示底部 page indicator。 */
    page_num = 0;
    page_curr = 0;

    menu_screen1 = lv_obj_create(parent);
    lv_obj_set_size(menu_screen1, lv_pct(100), LV_VER_RES - status_bar_height);
    lv_obj_set_style_bg_color(menu_screen1, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(menu_screen1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(menu_screen1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(menu_screen1, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_screen1, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_border_side(menu_screen1, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_screen1, 0, LV_PART_MAIN);
    lv_obj_align(menu_screen1, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* 第二页容器仍创建但永久隐藏，禁止 menu_btn_create 把第 10~18 项放进去。 */
    menu_screen2 = lv_obj_create(parent);
    lv_obj_set_size(menu_screen2, lv_pct(100), LV_VER_RES - status_bar_height);
    lv_obj_set_style_bg_color(menu_screen2, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(menu_screen2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(menu_screen2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(menu_screen2, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_screen2, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_border_side(menu_screen2, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_screen2, 0, LV_PART_MAIN);
    lv_obj_align(menu_screen2, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(menu_screen2, LV_OBJ_FLAG_HIDDEN);

    if(ui_test_a7682e() == false)
    {
        for(int i = 0; i < GET_BUFF_LEN(menu_btn_list); i++)
        {
            if(menu_btn_list[i].idx == SCREEN8_ID)
            {
                menu_btn_list[i].idx = SCREEN10_ID;
                menu_btn_list[i].name = "PCM5012";
                menu_btn_list[i].icon = &img_PCM5102;
            }
        }
    }

    /* 2026-09-21: 主菜单单页：所有 menu_btn 全部放到 menu_screen1，不再分两页。 */
    for(int i = 0; i < MENU_BTN_NUM; i++) {
        menu_btn_create(menu_screen1, &menu_btn_list[i]);
    }

    /* 单页：底部 page indicator (ui_Panel4) 不创建——只在原 MENU_BTN_NUM > 9 时才创建，
     * 现在 page_num = 0，第二页已禁用，indicator 无意义。 */

    /* 主页面最底细黑线：试验减轻 EPD 全刷时底边竖向光柱（Mura）。ALIGN_BOTTOM y=0。 */
    lv_obj_t *menu_bottom_line = lv_obj_create(parent);
    lv_obj_remove_style_all(menu_bottom_line);
    lv_obj_set_size(menu_bottom_line, LV_HOR_RES, 1);
    lv_obj_set_style_bg_color(menu_bottom_line, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu_bottom_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_bottom_line, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu_bottom_line, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(menu_bottom_line, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_move_foreground(menu_bottom_line);
}

static void entry0(void) {
    ui_get_gesture_dir = menu_get_gesture_dir;
    lv_timer_resume(touch_chk_timer);
    /* 手势改由 w20_deckpro_shell_loop 内 ui_deckpro_poll_gesture 轮询；定时器保持暂停 */
    lv_timer_pause(touch_chk_timer);
    lv_timer_resume(taskbar_update_timer);

    menu_taskbar_refresh_all();
}
static void exit0(void) {
    ui_get_gesture_dir = NULL;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    ui_deckpro_gesture_reset();
#endif
    lv_timer_pause(touch_chk_timer);
    lv_timer_pause(taskbar_update_timer);
}
static void destroy0(void) {
    if(menu_taskbar) {
        lv_obj_del(menu_taskbar);
        menu_taskbar = NULL;
    }
}

static scr_lifecycle_t screen0 = {
    .create = create0,
    .entry = entry0,
    .exit  = exit0,
    .destroy = destroy0,
};
#endif
//************************************[ screen 1 ]****************************************** lora
// --------------------- screen 1 --------------------- lora
#if 1
static int lora_ui_test_mode = LORA_MODE_SEND; /* SEND / RECV / CW */
lv_obj_t * scr1_list;
static lv_obj_t *scr1_lab_buf[20];

static const char *lora_ui_mode_title(int mode)
{
    if (mode == LORA_MODE_CW)
        return "CW Test";
    if (mode == LORA_MODE_RECV)
        return "Recv Test";
    return "Send Test";
}

static void scr1_list_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    for(int i = 0; i < lv_obj_get_child_cnt(obj); i++) 
    {
        lv_obj_t * child = lv_obj_get_child(obj, i);
        if(lv_obj_check_type(child, &lv_label_class)) {
            char *str = lv_label_get_text(child);

            if(strcmp("- Send Test", str) == 0) {
                lora_ui_test_mode = LORA_MODE_SEND;
                scr_push_subscreen(SCREEN1_1_ID);
            } else if(strcmp("- Recv Test", str) == 0) {
                lora_ui_test_mode = LORA_MODE_RECV;
                scr_push_subscreen(SCREEN1_1_ID);
            } else if(strcmp("- CW Test", str) == 0) {
                lora_ui_test_mode = LORA_MODE_CW;
                scr_push_subscreen(SCREEN1_1_ID);
            }
        }
    }
}

static void scr1_item_create(const char *name, lv_event_cb_t cb)
{
    lv_obj_t * obj = lv_obj_class_create_obj(&lv_list_btn_class, scr1_list);
    lv_obj_class_init_obj(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *label = lv_label_create(obj);
    lv_label_set_text(label, name);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_set_height(obj, LV_VER_RES / 6);
    lv_obj_set_style_text_font(obj, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    /* 墨水屏：按下态不画 outline，减少松手前列表 invalidate 导致「先刷列表再进子页」 */
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, NULL); 
}

static void scr1_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        // ui_full_refresh();
        scr_mgr_pop(false);
    }
}

static void create1(lv_obj_t *parent) 
{
    scr1_list = lv_list_create(parent);
    lv_obj_set_size(scr1_list, lv_pct(93), lv_pct(91));
    lv_obj_align(scr1_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_set_style_bg_color(scr1_list, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_pad_top(scr1_list, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr1_list, 15, LV_PART_MAIN);
    lv_obj_set_style_radius(scr1_list, 0, LV_PART_MAIN);
    // lv_obj_set_style_outline_pad(scr1_list, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr1_list, 0, LV_PART_MAIN);
    // lv_obj_set_style_border_color(scr1_list, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(scr1_list, 0, LV_PART_MAIN);

    scr1_item_create("- Send Test", scr1_list_event);
    scr1_item_create("- Recv Test", scr1_list_event);
    scr1_item_create("- CW Test", scr1_list_event);

    // back
    scr_back_btn_create(parent, "Lora", scr1_btn_event_cb);
}

static void entry1(void) 
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (!ui_setting_get_lora_status()) {
        W20_UI_LOG("LoRa menu: power OFF — init skipped, Meshtastic keeps radio");
    } else {
        w20_lora_arbiter_request_deck();
    }
#endif
}
static void exit1(void) {
    /* push 到 Send/Recv/CW 时也会走 exit，不能在这里 request_mesh，否则子页刚拿到射频又被抢走。 */
}
static void destroy1(void) {
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    /* 真正离开 Lora 菜单（pop/switch 销毁）时才还给 Mesh */
    if (ui_setting_get_lora_status()) {
        w20_lora_arbiter_request_mesh();
    }
#endif
}

static scr_lifecycle_t screen1 = {
    .create = create1,
    .entry = entry1,
    .exit  = exit1,
    .destroy = destroy1,
};
#endif
// --------------------- screen 1.1 --------------------- LoRa Send/Recv Test
#if 1
static lv_obj_t *scr1_1_cont;
static lv_obj_t *lora_title_lab;
static lv_obj_t *lora_sig_lab;
static lv_obj_t *lora_lab_buf[11] = {0};
static lv_timer_t *lora_test_timer = NULL;
static int lora_cnt = 0;
static const int LORA_SEND_MAX_LINES = 11;
static const int LORA_RECV_MAX_LINES = 6;
static bool lora_test_tx_ui_reset = false;
/* 进子页时勿在 entry 同步 ensure_deck：否则 push 未完成/仍停在列表屏时墨水会先刷一帧列表。 */
static bool lora_test_radio_pending = false;
static bool lora_test_radio_ready = false;

static void lora_clear_lines(void)
{
    for(int i = 0; i < (int)GET_BUFF_LEN(lora_lab_buf); i++) {
        lv_label_set_text(lora_lab_buf[i], " ");
    }
}

static void scr1_1_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static bool lora_cw_ui_shown = false;

static bool lora_test_try_init_radio(lv_timer_t *t)
{
    if (!lora_test_radio_pending) {
        return lora_test_radio_ready;
    }
    lora_test_radio_pending = false;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (!w20_lora_arbiter_ensure_deck(800)) {
        W20_UI_LOG("LoRa test: Deck init FAILED");
        if (lora_lab_buf[0]) {
            lv_label_set_text(lora_lab_buf[0], "Deck init FAILED");
            ui_disp_full_refr();
        }
        if (t) {
            lv_timer_del(t);
            lora_test_timer = NULL;
        }
        return false;
    }
    W20_UI_LOG("LoRa test: Deck init OK");
#endif
    if (lora_ui_test_mode == LORA_MODE_SEND) {
        lora_test_tx_ui_reset = true;
        ui_lora_set_mode(LORA_MODE_SEND);
    } else if (lora_ui_test_mode == LORA_MODE_CW) {
        ui_lora_set_mode(LORA_MODE_CW);
    } else {
        ui_lora_set_recv_flag();
        ui_lora_set_mode(LORA_MODE_RECV);
    }
    lora_test_radio_ready = true;
    if (t) {
        lv_timer_set_period(t, 250);
    }
    return true;
}

static void lora_test_timer_event(lv_timer_t *t)
{
    if (!lora_test_try_init_radio(t)) {
        return;
    }
    if (lora_ui_test_mode == LORA_MODE_SEND) {
        /* 对齐 w20_meshtastic_lora2021：按 last_sent 变化刷 TX 行，不依赖 TX_DONE IRQ */
        static char last_tx[64] = {0};
        if (lora_test_tx_ui_reset) {
            lora_test_tx_ui_reset = false;
            memset(last_tx, 0, sizeof(last_tx));
        }
        ui_lora_send(NULL);
        const char *s = ui_lora_get_last_sent();
        if (s && s[0] != '\0' && strncmp(s, last_tx, sizeof(last_tx)) != 0) {
            strncpy(last_tx, s, sizeof(last_tx) - 1);
            last_tx[sizeof(last_tx) - 1] = '\0';
            if (lora_cnt >= LORA_SEND_MAX_LINES) {
                lora_clear_lines();
                lora_cnt = 0;
            }
            lv_label_set_text_fmt(lora_lab_buf[lora_cnt], "TX: %s", s);
            lora_cnt++;
            /* 墨水屏全刷 ~1.3s：每行都刷会卡死发包节奏，约 3s 刷一次 */
            static uint32_t s_last_tx_epd_ms = 0;
            uint32_t now = millis();
            if (lora_cnt == 1 || (now - s_last_tx_epd_ms) >= 3000) {
                s_last_tx_epd_ms = now;
                ui_disp_full_refr();
            }
        }
    } else if (lora_ui_test_mode == LORA_MODE_RECV) {
        ui_lora_recv_loop();
        const char *recv_info = NULL;
        int recv_rssi = 0;
        float recv_snr = 0.0f;
        if (ui_lora_get_recv(&recv_info, &recv_rssi, &recv_snr)) {
            ui_lora_set_recv_flag();
            if (lora_sig_lab) {
                lv_label_set_text_fmt(lora_sig_lab, "RSSI: %d  SNR: %.1f",
                                      recv_rssi, (double)recv_snr);
            }
            if (lora_cnt >= LORA_RECV_MAX_LINES) {
                lora_clear_lines();
                lora_cnt = 0;
            }
            lv_label_set_text_fmt(lora_lab_buf[lora_cnt], "RX: %s", recv_info);
            lora_cnt++;
            ui_disp_full_refr();
        }
    } else if (!lora_cw_ui_shown) {
        lora_cw_ui_shown = true;
        lv_label_set_text_fmt(lora_lab_buf[0], "CW ON @%.2fM %ddBm", (double)ui_lora_get_freq(),
                              LORA_TX_POWER_DBM);
        ui_disp_full_refr();
    }
}

static lv_obj_t * scr2_create_label(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, LV_HOR_RES - 26);
    lv_obj_set_style_text_font(label, FONT_BOLD_SIZE_15, LV_PART_MAIN);   
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}
static void create1_1(lv_obj_t *parent) 
{
    lora_title_lab = lv_label_create(parent);
    lv_obj_set_style_text_font(lora_title_lab, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_label_set_text(lora_title_lab, lora_ui_mode_title(lora_ui_test_mode));
    lv_obj_align(lora_title_lab, LV_ALIGN_TOP_MID, 0, 8);

    lora_sig_lab = lv_label_create(parent);
    lv_obj_set_style_text_font(lora_sig_lab, FONT_BOLD_SIZE_14, LV_PART_MAIN);
    lv_label_set_text(lora_sig_lab, "RSSI: --  SNR: --");
    lv_obj_align_to(lora_sig_lab, lora_title_lab, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    if(lora_ui_test_mode != LORA_MODE_RECV) {
        lv_obj_add_flag(lora_sig_lab, LV_OBJ_FLAG_HIDDEN);
    }

    scr1_1_cont = lv_obj_create(parent);
    lv_obj_set_size(scr1_1_cont, lv_pct(100), lv_pct(85));
    lv_obj_set_style_bg_color(scr1_1_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr1_1_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr1_1_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(scr1_1_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr1_1_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(scr1_1_cont, 13, LV_PART_MAIN);
    lv_obj_set_flex_flow(scr1_1_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr1_1_cont, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr1_1_cont, 5, LV_PART_MAIN);
    lv_obj_set_align(scr1_1_cont, LV_ALIGN_BOTTOM_MID);

    for(int i = 0; i < GET_BUFF_LEN(lora_lab_buf); i++){
        lora_lab_buf[i] = scr2_create_label(scr1_1_cont);
        lv_label_set_text(lora_lab_buf[i], " ");
    }

    lv_obj_t *lab = lv_label_create(parent);
    lv_obj_set_style_text_font(lab, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    lv_label_set_text_fmt(lab, "%.2fM", ui_lora_get_freq());
    lv_obj_align(lab, LV_ALIGN_TOP_RIGHT, -10, 10);

    // back
    scr_back_btn_create(parent, ("Lora"), scr1_1_btn_event_cb);
}
static void entry1_1(void) 
{
    if(lora_title_lab) {
        lv_label_set_text(lora_title_lab, lora_ui_mode_title(lora_ui_test_mode));
    }
    if(lora_sig_lab) {
        if(lora_ui_test_mode != LORA_MODE_RECV) {
            lv_obj_add_flag(lora_sig_lab, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(lora_sig_lab, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lora_sig_lab, "RSSI: --  SNR: --");
        }
    }
    lora_clear_lines();
    lora_cnt = 0;
    lora_cw_ui_shown = false;
    lora_test_radio_ready = false;
    lora_test_radio_pending = false;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (!ui_setting_get_lora_status()) {
        W20_UI_LOG("LoRa test: power OFF — static view, no refresh");
        return;
    }
#endif
    /* 先让 push + prepare_subscreen 刷出子页，再在短定时器里 init 射频，避免先刷一帧列表 */
    lora_test_radio_pending = true;
    if (lora_test_timer) {
        lv_timer_del(lora_test_timer);
        lora_test_timer = NULL;
    }
    lora_test_timer = lv_timer_create(lora_test_timer_event, 30, NULL);
}
static void exit1_1(void) {
    lora_test_radio_pending = false;
    lora_test_radio_ready = false;
    if(lora_test_timer) {
        lv_timer_del(lora_test_timer);
        lora_test_timer = NULL;
    }
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (ui_setting_get_lora_status()) {
        lora_sleep();
    }
#else
    lora_sleep();
#endif
}
static void destroy1_1(void) { }

static scr_lifecycle_t screen1_1 = {
    .create = create1_1,
    .entry = entry1_1,
    .exit  = exit1_1,
    .destroy = destroy1_1,
};
#endif
//************************************[ screen 2 ]****************************************** Setting
// --------------------- screen 2.1 --------------------- About System
#if 1
static lv_obj_t *scr2_1_cont;

static void scr2_1_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void create2_1(lv_obj_t *parent) 
{
    lv_obj_t *info = lv_label_create(parent);
    lv_obj_set_width(info, LV_HOR_RES * 0.9);
    lv_obj_set_style_text_color(info, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_text_font(info, &Font_Mono_Bold_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);

    String str = "";

    str += "                           \n";
    str += line_full_format(28, "SF Version:", ui_setting_get_sf_ver());
    str += "\n                           \n";

    str += line_full_format(28, "HD Version:", ui_setting_get_hd_ver());
    str += "\n                           \n";

    lv_label_set_text_fmt(info, "%s", str.c_str());
    
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 35);
    
    lv_obj_t *back2_1_label = scr_back_btn_create(parent, ("About System"), scr2_1_btn_event_cb);
}
static void entry2_1(void) 
{
}
static void exit2_1(void) {
}
static void destroy2_1(void) { }

static scr_lifecycle_t screen2_1 = {
    .create = create2_1,
    .entry = entry2_1,
    .exit  = exit2_1,
    .destroy = destroy2_1,
};
#endif
// --------------------- screen 2 --------------------- Setting
#if 1
static lv_obj_t *setting_list;
static lv_timer_t *setting_rgb_charge_timer = NULL;
static int setting_num = 0;
static int setting_page_num = 0;
static int setting_curr_page = 0;
static int8_t setting_rgb_charging = -1;

static const char *setting_rgb_mode_text(int mode)
{
    switch (mode) {
    case 1: return "R+";
    case 2: return "B+";
    case 3: return "G+";
    default: return "OFF";
    }
}

static ui_setting_handle setting_handle_list[] = {
    {.name = "Lighting Status",   .type=UI_SETTING_TYPE_SW,  .set_cb = ui_setting_set_led_status,   .get_cb = ui_setting_get_led_status},
    {.name = "Power GPS",        .type=UI_SETTING_TYPE_SW,  .set_cb = ui_setting_set_gps_status,   .get_cb = ui_setting_get_gps_status},
    {.name = "Power Lora",       .type=UI_SETTING_TYPE_SW,  .set_cb = ui_setting_set_lora_status,  .get_cb = ui_setting_get_lora_status},
    {.name = "RGB Status",       .type=UI_SETTING_TYPE_RGB, .set_mode_cb = ui_setting_set_rgb_mode, .get_mode_cb = ui_setting_get_rgb_mode},
    {.name = "About System",   .type=UI_SETTING_TYPE_SUB, .sub_id = SCREEN2_1_ID},
};

static bool setting_rgb_sync_charging(bool refresh)
{
    const bool charging = ui_battery_charging();
    const int8_t charging_state = charging ? 1 : 0;
    const bool state_changed = setting_rgb_charging != charging_state;
    setting_rgb_charging = charging_state;

    if ((charging && ui_setting_get_rgb_mode() != 0) || (!charging && state_changed)) {
        /* G+ is controlled by the charger hardware; software RGB channels stay off. */
        ui_setting_set_rgb_mode(0);
    }

    if (setting_list == NULL) {
        return state_changed;
    }

    for (int i = 0; i < setting_num; i++) {
        ui_setting_handle *h = &setting_handle_list[i];
        if (h->type == UI_SETTING_TYPE_RGB && h->st != NULL) {
            lv_label_set_text(h->st, setting_rgb_mode_text(charging ? 3 : ui_setting_get_rgb_mode()));
            if (refresh && state_changed) {
                ui_disp_full_refr();
            }
            break;
        }
    }
    return state_changed;
}

static void setting_rgb_charge_timer_event(lv_timer_t *t)
{
    (void)t;
    setting_rgb_sync_charging(true);
}

static void setting_item_create(int curr_apge);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
void ui_deckpro_setting_defer_cancel(void)
{
}

/** 取消延迟刷屏定时器；保留 list/label 指针让 inactive 状态下 handler 仍能更新 label。
 *  仅 destroy 时（widget 真释放）才彻底断开指针，避免 lv_obj_del 后 use-after-free。 */
static void setting_ui_detach(void)
{
    ui_deckpro_setting_defer_cancel();
}

void ui_deckpro_setting_refresh_labels(void)
{
    if (setting_list == NULL) {
        return;
    }
    for (int i = 0; i < setting_num; i++) {
        ui_setting_handle *h = &setting_handle_list[i];
        if (h->st == NULL) {
            continue;
        }
        switch (h->type) {
        case UI_SETTING_TYPE_SW:
            lv_label_set_text_fmt(h->st, "%s", h->get_cb() ? "ON" : "OFF");
            break;
        case UI_SETTING_TYPE_RGB:
            lv_label_set_text(h->st, setting_rgb_mode_text(ui_battery_charging() ? 3 : h->get_mode_cb()));
            break;
        default:
            break;
        }
    }
}

void ui_deckpro_setting_defer_refresh(void)
{
    /* LED/GPIO 侧写回 UI：仅在 Setting 页顶时刷墨，避免其它页误触发全刷。 */
    if (scr_mgr_top_id() != SCREEN2_ID) {
        return;
    }
    ui_deckpro_setting_refresh_labels();
    ui_disp_full_refr();
}
#endif

static void scr2_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void setting_scr_event(lv_event_t *e)
{
    lv_obj_t *tgt = (lv_obj_t *)e->target;
    ui_setting_handle *h = (ui_setting_handle *)e->user_data;

    if(e->code == LV_EVENT_CLICKED) {
        switch (h->type)
        {
        case UI_SETTING_TYPE_SW:
            W20_SETTING_LOG("setting SW click idx=%d get=%d", (int)(h - setting_handle_list), h->get_cb() ? 1 : 0);
            h->set_cb(!h->get_cb());
            W20_SETTING_LOG("setting SW after-set get=%d list=%p st=%p", h->get_cb() ? 1 : 0, (void*)setting_list, (void*)h->st);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
            /* 即使 setting_list=NULL(scr_mgr_inactive 后 widget 仍可能存活),
             * 同步刷 label 文本并立即刷墨，避免「点了没反应、退出再进才变」。 */
            if (h->st != nullptr) {
                lv_label_set_text_fmt(h->st, "%s", h->get_cb() ? "ON" : "OFF");
            }
            ui_disp_full_refr();
#else
            lv_label_set_text_fmt(h->st, "%s", (h->get_cb() ? "ON" : "OFF"));
            ui_disp_full_refr();
#endif
            break;
        case UI_SETTING_TYPE_RGB:
        {
            if (ui_battery_charging()) {
                setting_rgb_sync_charging(true);
                break;
            }
            int mode = h->get_mode_cb();
            W20_SETTING_LOG("setting RGB click idx=%d get=%d", (int)(h - setting_handle_list), mode);
            mode = (mode + 1) % 3;
            h->set_mode_cb(mode);
            W20_SETTING_LOG("setting RGB after-set get=%d list=%p st=%p", h->get_mode_cb(), (void*)setting_list, (void*)h->st);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
            if (h->st != nullptr) {
                lv_label_set_text(h->st, setting_rgb_mode_text(ui_battery_charging() ? 3 : h->get_mode_cb()));
            }
            ui_disp_full_refr();
#else
            lv_label_set_text(h->st, setting_rgb_mode_text(h->get_mode_cb()));
            ui_disp_full_refr();
#endif
            break;
        }
        case UI_SETTING_TYPE_SUB:
            scr_push_subscreen(h->sub_id);
            break;
        default:
            break;
        }
    }
}

static void setting_item_create(int curr_apge)
{
    int start = (curr_apge * SETTING_PAGE_MAX_ITEM);
    int end = start + SETTING_PAGE_MAX_ITEM;
    if(end > setting_num) end = setting_num;

    for(int i = start; i < end; i++) {
        ui_setting_handle *h = &setting_handle_list[i];
        

        switch (h->type)
        {
        case UI_SETTING_TYPE_SW:
            h->obj = lv_list_add_btn(setting_list, NULL, h->name);
            h->st = lv_label_create(h->obj);
            lv_obj_set_style_text_font(h->st, FONT_BOLD_SIZE_15, LV_PART_MAIN);
            lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text_fmt(h->st, "%s", (h->get_cb() ? "ON" : "OFF"));
            break;
        case UI_SETTING_TYPE_RGB:
            h->obj = lv_list_add_btn(setting_list, NULL, h->name);
            h->st = lv_label_create(h->obj);
            lv_obj_set_style_text_font(h->st, FONT_BOLD_SIZE_15, LV_PART_MAIN);
            lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_label_set_text(h->st, setting_rgb_mode_text(ui_battery_charging() ? 3 : h->get_mode_cb()));
            break;
        case UI_SETTING_TYPE_SUB:
            h->obj = lv_list_add_btn(setting_list, NULL, h->name);
            break;
        default:
            break;
        }

        // style：高度 35，文字在框内垂直居中；触控区域与可视高度一致并覆盖行间距
        lv_obj_set_height(h->obj, SETTING_ITEM_BTN_H);
        lv_obj_set_style_min_height(h->obj, SETTING_ITEM_BTN_H, LV_PART_MAIN);
        lv_obj_set_style_max_height(h->obj, SETTING_ITEM_BTN_H, LV_PART_MAIN);
        lv_obj_set_style_text_font(h->obj, FONT_BOLD_SIZE_15, LV_PART_MAIN);
        lv_obj_set_style_bg_color(h->obj, DECKPRO_COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_text_color(h->obj, DECKPRO_COLOR_FG, LV_PART_MAIN);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
        /* 墨水屏：按下不画 outline，避免进子页前多刷一帧当前 Setting 页 */
        lv_obj_set_style_outline_width(h->obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(h->obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(h->obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(h->obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(h->obj, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(h->obj, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(h->obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(h->obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(h->obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(h->obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_ext_click_area(h->obj, SETTING_ITEM_CLICK_EXT);
        /* list_btn 内主标题 label 也垂直居中；子控件不抢触控 */
        for (uint32_t ci = 0; ci < lv_obj_get_child_cnt(h->obj); ci++) {
            lv_obj_t *ch = lv_obj_get_child(h->obj, ci);
            if (lv_obj_check_type(ch, &lv_label_class)) {
                lv_obj_set_style_pad_top(ch, 0, LV_PART_MAIN);
                lv_obj_set_style_pad_bottom(ch, 0, LV_PART_MAIN);
                lv_obj_clear_flag(ch, LV_OBJ_FLAG_CLICKABLE);
                if (ch != h->st) {
                    lv_obj_set_style_text_align(ch, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
                }
            }
        }
        if (h->st != NULL) {
            lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
            lv_obj_add_flag(h->st, LV_OBJ_FLAG_EVENT_BUBBLE);
            lv_obj_clear_flag(h->st, LV_OBJ_FLAG_CLICKABLE);
        }
        lv_obj_add_event_cb(h->obj, setting_scr_event, LV_EVENT_CLICKED, (void *)h);
    }
}

static void create2(lv_obj_t *parent) 
{
    setting_list = lv_list_create(parent);
    lv_obj_set_size(setting_list, LV_HOR_RES, lv_pct(93));
    lv_obj_align(setting_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(setting_list, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_pad_top(setting_list, SETTING_ITEM_PAD_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(setting_list, SETTING_ITEM_PAD_ROW, LV_PART_MAIN);
    lv_obj_set_style_radius(setting_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(setting_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(setting_list, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(setting_list, 0, LV_PART_MAIN);

    setting_num = sizeof(setting_handle_list) / sizeof(setting_handle_list[0]);
    setting_page_num = setting_num / SETTING_PAGE_MAX_ITEM;

    setting_item_create(setting_curr_page);
    lv_obj_update_layout(setting_list);

    scr_back_btn_create(parent, ("Setting"), scr2_btn_event_cb);
}
static void entry2(void) {
    /* 与 volatile 状态同步到 GPIO（防止休眠 hold 等导致脚态与 UI 不一致） */
    ui_setting_set_led_status(ui_setting_get_led_status());
    setting_rgb_charging = -1;
    setting_rgb_sync_charging(false);
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    ui_deckpro_setting_refresh_labels();
    /* entry 已立即对齐文案，取消 set_led_status 带上的延时刷，避免进页后再全刷一帧。 */
    ui_deckpro_setting_defer_cancel();
#endif
    /* 充电态边沿由 ui_deckpro_poll_power_ui() 驱动 setting_rgb_sync_charging(true)，
     * 不恢复 500ms 无脑轮询，避免同文案反复全刷闪屏。 */
}
static void exit2(void)
{
    if (setting_rgb_charge_timer) {
        lv_timer_del(setting_rgb_charge_timer);
        setting_rgb_charge_timer = NULL;
    }
    setting_rgb_charging = -1;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    setting_ui_detach();
#endif
}
static void destroy2(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    setting_ui_detach();
#endif
}

static scr_lifecycle_t screen2 = {
    .create = create2,
    .entry = entry2,
    .exit  = exit2,
    .destroy = destroy2,
};
#endif
//************************************[ screen 3 ]****************************************** GPS
#if 1
#define line_max 23
static lv_obj_t *scr3_cont;
static lv_timer_t *GPS_loop_timer = NULL;
static bool s_gps_deck_pending = false;
static uint32_t s_gps_ui_log_ms = 0;
static uint32_t s_gps_pending_log_ms = 0;

static bool gps_set_line(lv_obj_t *label, const char *str1, const char *str2)
{
    if (!label) {
        return false;
    }
    char next[40];
    int w2 = (int)strlen(str2);
    int w1 = line_max - w2;
    if (w1 < 1) {
        w1 = 1;
    }
    lv_snprintf(next, sizeof(next), "%-*s%-*s", w1, str1, w2, str2);
    const char *cur = lv_label_get_text(label);
    if (cur && strcmp(cur, next) == 0) {
        return false;
    }
    lv_label_set_text(label, next);
    return true;
}

static lv_obj_t * scr3_create_label(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(90));
    lv_obj_set_style_text_font(label, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);   
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_border_side(label, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    return label;
}

static bool scr3_GPS_updata(void)
{
    double lat = 0, lon = 0, speed = 0, alt = 0, course = 0;
    float hdop = 0.0f;
    uint32_t vsat = 0;
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, min = 0, sec = 0;
    const bool fix = ui_gps_get_fix_valid();

    ui_gps_get_coord(&lat, &lon);
    ui_gps_get_data(&year, &month, &day);
    ui_gps_get_time(&hour, &min, &sec);
    ui_gps_get_satellites(&vsat);
    ui_gps_get_speed(&speed);
    ui_gps_get_altitude(&alt);
    ui_gps_get_course(&course);
    ui_gps_get_hdop(&hdop);

    char buf[32];
    bool changed = false;

    changed |= gps_set_line(label_list[0], "Fix:", fix ? "OK" : "NoFix");

    lv_snprintf(buf, sizeof(buf), "%.5f", lat);
    changed |= gps_set_line(label_list[1], "Lat:", buf);

    lv_snprintf(buf, sizeof(buf), "%.5f", lon);
    changed |= gps_set_line(label_list[2], "Lon:", buf);

    lv_snprintf(buf, sizeof(buf), "%.1fm", alt);
    changed |= gps_set_line(label_list[3], "Alt:", buf);

    lv_snprintf(buf, sizeof(buf), "%.1fkm/h", speed);
    changed |= gps_set_line(label_list[4], "Speed:", buf);

    lv_snprintf(buf, sizeof(buf), "%.0fdeg", course);
    changed |= gps_set_line(label_list[5], "Course:", buf);

    lv_snprintf(buf, sizeof(buf), "%lu", (unsigned long)vsat);
    changed |= gps_set_line(label_list[6], "Sats:", buf);

    lv_snprintf(buf, sizeof(buf), "%.1f", hdop);
    changed |= gps_set_line(label_list[7], "HDOP:", buf);

    if (year > 0) {
        lv_snprintf(buf, sizeof(buf), "%04u-%02u-%02u", year, month, day);
    } else {
        lv_snprintf(buf, sizeof(buf), "----");
    }
    changed |= gps_set_line(label_list[8], "Date:", buf);

    lv_snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hour, min, sec);
    changed |= gps_set_line(label_list[9], "Time:", buf);

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    const uint32_t now = millis();
    if (now - s_gps_ui_log_ms >= 5000) {
        s_gps_ui_log_ms = now;
        W20_GPS_LOG("[GPS-UI] fix=%d sats=%lu lat=%.5f lon=%.5f\r\n", fix ? 1 : 0, (unsigned long)vsat, lat, lon);
    }
#endif
    return changed;
}

static void GPS_loop_timer_event(lv_timer_t * t)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (s_gps_deck_pending) {
        if (!ui_setting_get_gps_status()) {
            s_gps_deck_pending = false;
            W20_UI_LOG("GPS screen: power OFF — static view");
            lv_timer_set_period(t, 5000);
            return;
        }
        /* 后台推进 Mesh→Deck；页面已是最终布局，成功后再刷一次 */
        w20_gps_arbiter_request_deck();
        w20_gps_arbiter_tick();
        if (w20_gps_arbiter_ensure_deck(0)) {
            s_gps_deck_pending = false;
            W20_GPS_LOGLN("[GPS-UI] handoff OK -> ui_gps_task_resume, timer 5s");
            ui_gps_task_resume();
            (void)scr3_GPS_updata();
            ui_disp_full_refr();
            lv_timer_set_period(t, 5000);
            return;
        }
        const uint32_t now = millis();
        if (now - s_gps_pending_log_ms >= 1000) {
            s_gps_pending_log_ms = now;
            W20_GPS_LOGLN("[GPS-UI] handoff pending (Mesh->Deck)...");
        }
        return;
    }
#endif
    /* 5s 轮询：无参数变化不刷墨 */
    if (scr3_GPS_updata()) {
        ui_disp_full_refr();
    }
}

static void scr3_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
        W20_UI_LOG("scr3 GPS back -> pop menu");
#endif
        scr_mgr_pop(false);
    }
}

static void create3(lv_obj_t *parent) 
{   
    scr3_cont = lv_obj_create(parent);
    lv_obj_set_size(scr3_cont, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_color(scr3_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr3_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr3_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(scr3_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr3_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(scr3_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr3_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr3_cont, 0, LV_PART_MAIN);
    lv_obj_set_align(scr3_cont, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_flex_flow(scr3_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr3_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    for(int i = 0; i < sizeof(label_list) / sizeof(label_list[0]); i++) {
        label_list[i] = scr3_create_label(scr3_cont);
        lv_label_set_text(label_list[i], " ");
    }

    lv_obj_t *back3_label = scr_back_btn_create(parent, ("GPS"), scr3_btn_event_cb);
}
static void entry3(void) 
{
    s_gps_deck_pending = false;
    if (GPS_loop_timer) {
        lv_timer_del(GPS_loop_timer);
        GPS_loop_timer = NULL;
    }
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (!ui_setting_get_gps_status()) {
        W20_UI_LOG("GPS screen: power OFF — static view, no refresh");
        (void)scr3_GPS_updata();
        return;
    }
    /* 直接画最终参数页（NoFix/0）；Mesh 交接在后台完成后再刷一次 */
    (void)scr3_GPS_updata();
    s_gps_deck_pending = true;
    s_gps_pending_log_ms = 0;
    W20_GPS_LOGLN("[GPS-UI] entry3: request_deck, pending=1");
    w20_gps_arbiter_request_deck();
    GPS_loop_timer = lv_timer_create(GPS_loop_timer_event, 50, NULL);
#else
    (void)scr3_GPS_updata();
    ui_gps_task_resume();
    GPS_loop_timer = lv_timer_create(GPS_loop_timer_event, 5000, NULL);
#endif
}
static void exit3(void) {
    s_gps_deck_pending = false;
    if(GPS_loop_timer) {
        lv_timer_del(GPS_loop_timer);
        GPS_loop_timer = NULL;
    }
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (ui_setting_get_gps_status()) {
        W20_GPS_LOGLN("[GPS-UI] exit3: request_mesh");
        ui_gps_task_suspend();
        w20_gps_arbiter_request_mesh();
    }
#else
    ui_gps_task_suspend();
#endif
}
static void destroy3(void) { }

static scr_lifecycle_t screen3 = {
    .create = create3,
    .entry = entry3,
    .exit  = exit3,
    .destroy = destroy3,
};

#undef line_max

#endif
//************************************[ screen 4 ]****************************************** Wifi Scan
/** 屏显与 SoftAP 均为 Mesh-XXXXXX（M 大写）；兼容旧版 mesh- 前缀。 */
static void format_mesh_ap_display(const char *apSsid, char *out, size_t outLen)
{
    if (!out || outLen == 0)
        return;
    if (!apSsid || apSsid[0] == '\0') {
        snprintf(out, outLen, "Mesh-...");
        return;
    }
    if (strncmp(apSsid, "mesh-", 5) == 0) {
        snprintf(out, outLen, "Mesh-%s", apSsid + 5);
    } else {
        snprintf(out, outLen, "%s", apSsid);
    }
}

// --------------------- screen 4 --------------------- WIFI
#if 1
lv_obj_t * scr4_list;
static lv_obj_t *scr4_lab_buf[20];

static void scr4_list_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *obj = lv_event_get_target(e);
    /* 清按下态但不立刻刷父页：nav_hold 挡住抬手→进页之间的墨水 */
    lv_obj_clear_state(obj, LV_STATE_PRESSED);
    for (int i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        lv_obj_t *child = lv_obj_get_child(obj, i);
        if (lv_obj_check_type(child, &lv_label_class)) {
            char *str = lv_label_get_text(child);
            int push_id = -1;
            if (strcmp("- WIFI Config", str) == 0) {
                push_id = SCREEN4_1_ID;
            } else if (strcmp("- WIFI Scan", str) == 0) {
                push_id = SCREEN4_2_ID;
            } else if (strcmp("- WiFi OTA", str) == 0) {
                W20_WIFI_LOGLN("[W20UI] WIFI list -> WiFi OTA");
                push_id = SCREEN4_3_ID;
            }
            W20_WIFI_LOG("[W20UI] WIFI list click %s\n", str);
            if (push_id >= 0) {
                scr_push_wifi_subscreen(push_id);
            }
        }
    }
}

static void scr4_item_create(const char *name, lv_event_cb_t cb)
{
    lv_obj_t * obj = lv_obj_class_create_obj(&lv_list_btn_class, scr4_list);
    lv_obj_class_init_obj(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);

    lv_obj_t *label = lv_label_create(obj);
    lv_label_set_text(label, name);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_set_height(obj, LV_VER_RES / 6);
    lv_obj_set_style_text_font(obj, FONT_BOLD_SIZE_15, LV_PART_MAIN);
    // lv_obj_set_style_bg_color(obj, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    // lv_obj_set_style_text_color(obj, lv_color_hex(EPD_COLOR_FG), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(obj, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, NULL); 
}

static void wifi_menu_power_off_deferred_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    ui_wifi_menu_power_off();
    ui_deckpro_resume_aux_timers();
}

static void scr4_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        /* 先切 LVGL 并刷主菜单；WiFi 停服延后，避免 touch 回调内阻塞 + PART 刷与 FULL 刷竞态。 */
        ui_deckpro_pause_aux_timers();
        w20_deckpro_epd_drain();
        scr_mgr_pop(false);
        lv_timer_create(wifi_menu_power_off_deferred_cb, 50, NULL);
    }
}

static void create4(lv_obj_t *parent) 
{
    /* 先画返回栏，再放列表；列表顶部留空，避免第一项 WIFI Scan 与返回点击区重叠 */
    scr_back_btn_create(parent, "WIFI", scr4_btn_event_cb);

    scr4_list = lv_list_create(parent);
    lv_obj_set_size(scr4_list, lv_pct(93), lv_pct(86));
    lv_obj_align(scr4_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(scr4_list, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr4_list, 15, LV_PART_MAIN);
    lv_obj_set_style_radius(scr4_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr4_list, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(scr4_list, 0, LV_PART_MAIN);

    scr4_item_create("- WIFI Scan", scr4_list_event);
    scr4_item_create("- WIFI Config", scr4_list_event);
    scr4_item_create("- WiFi OTA", scr4_list_event);
}

static void entry4(void) 
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    ui_wifi_menu_session_begin();
#endif
    /* 仅展示子菜单，不在此上电 WiFi（避免与已启用的 BLE 冲突导致重启）。
     * 实际用网在 WIFI Config / Scan / OTA 子页再 ui_wifi_menu_power_on()。 */
}
static void exit4(void) {
    ui_wifi_provisioning_set_ui_callback(NULL);
    /* push 子页时 scr_mgr 也会 inactive 本屏，不能在此关 AP；仅在 scr4 返回键离开 WiFi 菜单时 power_off */
}
static void destroy4(void) { }

static scr_lifecycle_t screen4 = {
    .create = create4,
    .entry = entry4,
    .exit  = exit4,
    .destroy = destroy4,
};
#endif
// --------------------- screen 4.2 --------------------- Wifi Config
#if 1
static lv_obj_t *wifi_cfg_cont;
static lv_obj_t *wifi_cfg_ssid_lab;
static lv_obj_t *wifi_cfg_pwd_lab;
static lv_obj_t *wifi_cfg_status_lab;
static lv_obj_t *wifi_cfg_ip_lab;
static lv_obj_t *wifi_cfg_rssi_lab;
static lv_obj_t *wifi_cfg_disc_btn = NULL;
static lv_obj_t *wifi_cfg_disc_lab = NULL;
static char wifi_cfg_last_line[5][64] = {{0}};
static char wifi_cfg_disc_cache[24] = {0};
static lv_timer_t *wifi_cfg_status_timer = NULL;
static lv_timer_t *wifi_cfg_deferred_timer = NULL;
static lv_timer_t *wifi_cfg_epd_pause_timer = NULL;
static bool wifi_cfg_epd_paused = false;
static unsigned long wifi_cfg_last_async_ms = 0;

static bool wifi_cfg_sta_linked(void)
{
    char ip[24] = {0};
    if (!ui_wifi_is_connected())
        return false;
    ui_wifi_get_ip(ip, sizeof(ip));
    return strcmp(ip, "0.0.0.0") != 0;
}

/** 量化 RSSI，避免 ±1dBm 抖动触发墨水屏刷新 */
static int wifi_cfg_rssi_stable(void)
{
    const int rssi = ui_wifi_get_sta_rssi();
    if (rssi >= 0) {
        return 0;
    }
    return ((rssi - 2) / 5) * 5;
}

static void wifi_cfg_set_line_if_changed(lv_obj_t *lab, char *cache, size_t cache_len, const char *text)
{
    if (strncmp(cache, text, cache_len) == 0)
        return;
    strncpy(cache, text, cache_len - 1);
    cache[cache_len - 1] = '\0';
    lv_label_set_text(lab, text);
}

/** SoftAP 配网时暂停 EPD flush：墨水 SPI 与 SoftAP 抢总线会导致门户页卡死/连不上。 */
static void wifi_cfg_set_epd_paused(bool pause)
{
    /* 与 factory 侧对齐：provTask 可能单独 suppress，退出时务必 resume */
    if (pause == wifi_cfg_epd_paused && pause == factory_epd_flush_suppressed()) {
        return;
    }
    wifi_cfg_epd_paused = pause;
    factory_set_epd_flush_suppressed(pause);
    W20_WIFI_LOG("[WiFiCfg] EPD flush %s (SoftAP portal)\n", pause ? "SUPPRESSED" : "resumed");
}

static void wifi_cfg_cancel_epd_pause_timer(void)
{
    if (wifi_cfg_epd_pause_timer) {
        lv_timer_del(wifi_cfg_epd_pause_timer);
        wifi_cfg_epd_pause_timer = NULL;
    }
}

static void wifi_cfg_epd_pause_timer_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    wifi_cfg_epd_pause_timer = NULL;
    if (scr_mgr_top_id() != SCREEN4_1_ID) {
        return;
    }
    const ui_wifi_prov_state_t st = ui_wifi_provisioning_get_state();
    if (ui_wifi_provisioning_active() || st == UI_WIFI_PROV_AP || st == UI_WIFI_PROV_CONNECTING ||
        st == UI_WIFI_PROV_STARTING) {
        wifi_cfg_set_epd_paused(true);
    }
}

/** SoftAP 起机/门户期尽快停 EPD：墨水 SPI 与 SoftAP 同抢会拖垮 HTTP/DNS。
 *  先留短窗口刷出「AP/Web」文案，再抑制（原先等 AP 后 2s 太晚）。 */
static void wifi_cfg_arm_epd_pause_soon(void)
{
    if (wifi_cfg_epd_paused) {
        return;
    }
    const ui_wifi_prov_state_t st = ui_wifi_provisioning_get_state();
    if (st != UI_WIFI_PROV_AP && st != UI_WIFI_PROV_CONNECTING && st != UI_WIFI_PROV_STARTING) {
        return;
    }
    if (wifi_cfg_epd_pause_timer) {
        lv_timer_reset(wifi_cfg_epd_pause_timer);
        return;
    }
    /* SoftAP 指引 FULL≈1.3s；原先 AP 态仅 400ms 会在 Disconnect→SoftAP 后提前停墨，屏仍停在旧「已连接」 */
    const uint32_t delay_ms = 1800;
    wifi_cfg_epd_pause_timer = lv_timer_create(wifi_cfg_epd_pause_timer_cb, delay_ms, NULL);
    lv_timer_set_repeat_count(wifi_cfg_epd_pause_timer, 1);
}

static void wifi_cfg_refresh_disc_btn(bool staLinked)
{
    if (!wifi_cfg_disc_btn || !wifi_cfg_disc_lab) {
        return;
    }
    const char *txt = staLinked ? "Disconnect WiFi" : "Reopen SoftAP";
    if (strncmp(wifi_cfg_disc_cache, txt, sizeof(wifi_cfg_disc_cache)) != 0) {
        strncpy(wifi_cfg_disc_cache, txt, sizeof(wifi_cfg_disc_cache) - 1);
        wifi_cfg_disc_cache[sizeof(wifi_cfg_disc_cache) - 1] = '\0';
        lv_label_set_text(wifi_cfg_disc_lab, txt);
    }
    lv_obj_clear_flag(wifi_cfg_disc_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_cfg_disc_btn, LV_OBJ_FLAG_CLICKABLE);
}

static void wifi_cfg_update_status(void)
{
    char apRaw[24] = {0};
    char apDisp[24] = {0};
    char line0[48] = {0};
    char line1[48] = {0};
    char line2[64] = {0};
    char line3[48] = {0};
    char line4[32] = {0};
    const bool staLinked = wifi_cfg_sta_linked();
    const ui_wifi_prov_state_t provState = ui_wifi_provisioning_get_state();
    const bool apPortal = ui_wifi_provisioning_active() || provState == UI_WIFI_PROV_AP ||
                          provState == UI_WIFI_PROV_CONNECTING || provState == UI_WIFI_PROV_STARTING;
    /* Disconnect(hold-offline) 后即使仍标 CONNECTED / 有缓存 IP，也不当「已连路由器」 */
    const bool routerUp =
        staLinked || (provState == UI_WIFI_PROV_CONNECTED && !ui_wifi_user_hold_offline() && !apPortal &&
                      ui_wifi_is_connected());

    /* SoftAP 已固定 192.168.4.1 且墨水已停：稳定态跳过；连上/失败再改文案 */
    if (wifi_cfg_epd_paused && apPortal && !staLinked && provState == UI_WIFI_PROV_AP &&
        !ui_wifi_provisioning_connect_failed()) {
        return;
    }

    ui_wifi_provisioning_get_ap_ssid(apRaw, sizeof(apRaw));
    format_mesh_ap_display(apRaw, apDisp, sizeof(apDisp));

    if (apPortal) {
        /* STA 已拿到 IP：文案先切「已连接」，但 SoftAP 未拆前不恢复 EPD（避免与门户抢 SPI） */
        if (staLinked && (provState == UI_WIFI_PROV_CONNECTING || provState == UI_WIFI_PROV_CONNECTED)) {
            char sta[33] = {0};
            char ip[24] = {0};
            ui_wifi_get_sta_ssid(sta, sizeof(sta));
            if (sta[0] == '\0') {
                ui_wifi_get_saved_ssid(sta, sizeof(sta));
            }
            ui_wifi_get_ip(ip, sizeof(ip));
            if ((!ip[0] || strcmp(ip, "0.0.0.0") == 0)) {
                (void)ui_wifi_provisioning_get_last_sta_ip(ip, sizeof(ip));
            }
            if (sta[0]) {
                snprintf(line0, sizeof(line0), "WiFi: %s", sta);
            } else {
                snprintf(line0, sizeof(line0), "WiFi: connected");
            }
            snprintf(line1, sizeof(line1), "Web: (router linked)");
            snprintf(line2, sizeof(line2), "Connected");
            snprintf(line3, sizeof(line3), "IP: %s", (ip[0] && strcmp(ip, "0.0.0.0") != 0) ? ip : "...");
            snprintf(line4, sizeof(line4), "RSSI: %d dBm", wifi_cfg_rssi_stable());
        } else {
            if (provState == UI_WIFI_PROV_AP || provState == UI_WIFI_PROV_CONNECTING ||
                provState == UI_WIFI_PROV_STARTING) {
                wifi_cfg_arm_epd_pause_soon();
            }
            snprintf(line0, sizeof(line0), "AP: %s", apDisp);
            snprintf(line1, sizeof(line1), "Web: http://192.168.4.1");
            if (provState == UI_WIFI_PROV_CONNECTING) {
                snprintf(line2, sizeof(line2), "Connecting...");
                snprintf(line3, sizeof(line3), "Portal held");
                snprintf(line4, sizeof(line4), "RSSI: --");
            } else if (ui_wifi_provisioning_connect_failed()) {
                snprintf(line2, sizeof(line2), "Connection failed. Retry!");
                snprintf(line3, sizeof(line3), "Reopen SoftAP / retry");
                snprintf(line4, sizeof(line4), "RSSI: --");
            } else {
                snprintf(line2, sizeof(line2), "Phone join AP, open Web");
                snprintf(line3, sizeof(line3), "Stay on this page");
                snprintf(line4, sizeof(line4), "RSSI: --");
            }
        }
    } else if (routerUp) {
        wifi_cfg_cancel_epd_pause_timer();
        wifi_cfg_set_epd_paused(false);
        char sta[33] = {0};
        char ip[24] = {0};
        ui_wifi_get_sta_ssid(sta, sizeof(sta));
        if (sta[0] == '\0') {
            ui_wifi_get_saved_ssid(sta, sizeof(sta));
        }
        ui_wifi_get_ip(ip, sizeof(ip));
        /* SoftAP 拆掉后 STA 重连瞬间 localIP 常为 0：优先用门户已拿到的缓存 IP */
        if ((!ip[0] || strcmp(ip, "0.0.0.0") == 0)) {
            (void)ui_wifi_provisioning_get_last_sta_ip(ip, sizeof(ip));
        }
        const bool haveIp = (ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0);
        /* 无有效 IP 时不刷「getting IP / IP:...」，等 DHCP 或缓存到位后一次切到 Connected */
        if (!haveIp) {
            return;
        }
        if (sta[0]) {
            snprintf(line0, sizeof(line0), "WiFi: %s", sta);
        } else {
            snprintf(line0, sizeof(line0), "WiFi: connected");
        }
        snprintf(line1, sizeof(line1), "Web: (router linked)");
        snprintf(line2, sizeof(line2), "Connected");
        snprintf(line3, sizeof(line3), "IP: %s", ip);
        snprintf(line4, sizeof(line4), "RSSI: %d dBm", wifi_cfg_rssi_stable());
    } else if (provState == UI_WIFI_PROV_CONNECTING) {
        wifi_cfg_arm_epd_pause_soon();
        snprintf(line0, sizeof(line0), "AP: %s", apDisp);
        snprintf(line1, sizeof(line1), "Web: http://192.168.4.1");
        snprintf(line2, sizeof(line2), "Connecting...");
        snprintf(line3, sizeof(line3), "Portal held");
        snprintf(line4, sizeof(line4), "RSSI: --");
    } else {
        wifi_cfg_cancel_epd_pause_timer();
        wifi_cfg_set_epd_paused(false);
        snprintf(line0, sizeof(line0), "AP: %s", apDisp);
        snprintf(line1, sizeof(line1), "Web: http://192.168.4.1");
        snprintf(line2, sizeof(line2), "Phone join AP, open Web");
        snprintf(line3, sizeof(line3), "Stay on this page");
        snprintf(line4, sizeof(line4), "RSSI: --");
    }

    wifi_cfg_set_line_if_changed(wifi_cfg_ssid_lab, wifi_cfg_last_line[0], sizeof(wifi_cfg_last_line[0]), line0);
    wifi_cfg_set_line_if_changed(wifi_cfg_pwd_lab, wifi_cfg_last_line[1], sizeof(wifi_cfg_last_line[1]), line1);
    wifi_cfg_set_line_if_changed(wifi_cfg_status_lab, wifi_cfg_last_line[2], sizeof(wifi_cfg_last_line[2]), line2);
    wifi_cfg_set_line_if_changed(wifi_cfg_ip_lab, wifi_cfg_last_line[3], sizeof(wifi_cfg_last_line[3]), line3);
    wifi_cfg_set_line_if_changed(wifi_cfg_rssi_lab, wifi_cfg_last_line[4], sizeof(wifi_cfg_last_line[4]), line4);
    wifi_cfg_refresh_disc_btn(staLinked);
}

static void wifi_cfg_status_timer_cb(lv_timer_t *timer)
{
    const ui_wifi_prov_state_t provState = ui_wifi_provisioning_get_state();
    const bool softApPortal = ui_wifi_provisioning_active() || provState == UI_WIFI_PROV_AP ||
                              provState == UI_WIFI_PROV_CONNECTING || provState == UI_WIFI_PROV_STARTING;
    const bool connecting = provState == UI_WIFI_PROV_CONNECTING && !wifi_cfg_sta_linked();
    const bool waitingIp = (provState == UI_WIFI_PROV_CONNECTED) && !wifi_cfg_sta_linked();
    const bool stableSta =
        wifi_cfg_sta_linked() && !ui_wifi_provisioning_active() && provState != UI_WIFI_PROV_CONNECTING;

    /* SoftAP 门户 + 墨水暂停：仍允许改文案，但拉长周期，避免刷屏 */
    if (softApPortal && wifi_cfg_epd_paused) {
        wifi_cfg_update_status();
        lv_timer_set_period(timer, waitingIp || connecting ? 2000 : 60000);
        return;
    }

    wifi_cfg_update_status();
    if (waitingIp) {
        /* 配网成功后等 DHCP：加快刷新，避免 IP 一直 ... */
        lv_timer_set_period(timer, 800);
    } else if (softApPortal) {
        lv_timer_set_period(timer, 30000);
    } else if (stableSta) {
        lv_timer_set_period(timer, 20000);
    } else if (connecting) {
        lv_timer_set_period(timer, 2000);
    } else {
        lv_timer_set_period(timer, 8000);
    }
}

static void wifi_cfg_kick_status_timer_soon(void)
{
    if (!wifi_cfg_status_timer) {
        return;
    }
    /* 配网结束/连上后立刻跑一轮，避免卡在 SoftAP 的 30s 周期 */
    lv_timer_set_period(wifi_cfg_status_timer, 200);
    lv_timer_reset(wifi_cfg_status_timer);
}

static void wifi_cfg_async_update(void *user_data)
{
    (void)user_data;
    wifi_cfg_update_status();
}

static void wifi_cfg_on_prov_change(void)
{
    const ui_wifi_prov_state_t st = ui_wifi_provisioning_get_state();
    const bool staLinked = wifi_cfg_sta_linked();
    /* SoftAP 已拆 / 终态：立刻恢复 EPD 并刷 UI */
    const bool terminal = (st == UI_WIFI_PROV_CONNECTED || st == UI_WIFI_PROV_FAILED || st == UI_WIFI_PROV_IDLE ||
                           ui_wifi_provisioning_connect_failed());
    const bool portalGone = !ui_wifi_provisioning_active() && st != UI_WIFI_PROV_AP &&
                            st != UI_WIFI_PROV_CONNECTING && st != UI_WIFI_PROV_STARTING;
    /* STA 已拿到 IP 但 SoftAP 仍 hold：只更新文案，不恢复 EPD（避免与门户抢 SPI） */
    const bool staSuccessHold = staLinked && st == UI_WIFI_PROV_CONNECTING && ui_wifi_provisioning_active();

    const unsigned long now = millis();
    if (terminal || portalGone) {
        wifi_cfg_cancel_epd_pause_timer();
        wifi_cfg_set_epd_paused(false);
        wifi_cfg_last_async_ms = now;
        wifi_cfg_kick_status_timer_soon();
        lv_async_call(wifi_cfg_async_update, NULL);
        return;
    }
    if (staSuccessHold) {
        wifi_cfg_last_async_ms = now;
        lv_async_call(wifi_cfg_async_update, NULL);
        return;
    }

    /* SoftAP 期间限制异步刷 UI，避免 Clients/状态抖动排队 invalidate */
    const unsigned long minGap = wifi_cfg_epd_paused ? 5000UL : 800UL;
    if (wifi_cfg_last_async_ms != 0 && (now - wifi_cfg_last_async_ms) < minGap) {
        return;
    }
    wifi_cfg_last_async_ms = now;
    lv_async_call(wifi_cfg_async_update, NULL);
}

static void wifi_cfg_disconnect_btn_cb(lv_event_t *e)
{
    if (e->code != LV_EVENT_CLICKED) {
        return;
    }

    const ui_wifi_prov_state_t provState = ui_wifi_provisioning_get_state();
    const bool portalUp = ui_wifi_provisioning_active() || provState == UI_WIFI_PROV_AP ||
                          provState == UI_WIFI_PROV_CONNECTING || provState == UI_WIFI_PROV_STARTING;

    /* 已连路由器 → Disconnect：断 STA + hold-offline，强制 SoftAP 重新配其它 WiFi。
     * （配网成功后的「保留 STA」与此无关；仅用户主动 Disconnect 才拆网重配。） */
    ui_wifi_user_hold_offline_set(true);

    if (portalUp) {
        /* SoftAP 已在：切勿 stop/restart；只保证 STA 已断，保持当前门户。 */
        ui_wifi_force_drop_sta_for_softap();
        W20_WIFI_LOGLN("[WiFiCfg] portal already up → keep SoftAP (no restart)");
        wifi_cfg_update_status();
        ui_disp_full_refr();
        return;
    }

    ui_wifi_force_drop_sta_for_softap();
    delay(80);

    W20_WIFI_LOGLN("[WiFiCfg] Disconnect → SoftAP re-provision (join other WiFi)");
    ui_wifi_provisioning_start(true);
    wifi_cfg_update_status();
    ui_disp_full_refr();
}

static void wifi_cfg_deferred_start_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    wifi_cfg_deferred_timer = NULL;

    if (scr_mgr_top_id() != SCREEN4_1_ID) {
        ui_deckpro_resume_aux_timers();
        return;
    }

    /* 已连路由器：只显示状态，不拆 STA、不起 SoftAP。
     * 开机自动重连中（有保存 SSID、非 hold-offline）：也不起 SoftAP，否则会掐掉正在连的 STA。
     * 未配网 / 用户 Disconnect hold-offline：才起 SoftAP。 */
    char savedSsid[33] = {0};
    ui_wifi_get_saved_ssid(savedSsid, (int)sizeof(savedSsid));
    const bool hasSaved = (savedSsid[0] != '\0');
    const bool holdOff = ui_wifi_user_hold_offline();
    const bool needSoftAp = !ui_wifi_sta_has_valid_ip() && (holdOff || !hasSaved);
    ui_wifi_provisioning_start(needSoftAp);
    wifi_cfg_update_status();
    if (needSoftAp) {
        ui_disp_full_refr(); /* 先刷 SoftAP 指引，arm_epd_pause 会随后停墨 */
    }
    /* SoftAP 配网期间勿开 taskbar 秒级刷新；已连/自动重连中可恢复。 */
    if (!needSoftAp) {
        ui_deckpro_resume_aux_timers();
    }
}

static void scr4_1_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        if (wifi_cfg_status_timer) {
            lv_timer_pause(wifi_cfg_status_timer);
        }
        wifi_cfg_cancel_epd_pause_timer();
        wifi_cfg_set_epd_paused(false);
        scr_mgr_pop(false);
        ui_deckpro_resume_aux_timers();
        /* 配网成功/断连后返回列表：强制全刷，避免停墨残留导致「未刷新」 */
        ui_disp_full_refr();
    }
}

static void create4_1(lv_obj_t *parent) 
{
    wifi_cfg_cont = lv_obj_create(parent);
    lv_obj_set_size(wifi_cfg_cont, lv_pct(96), lv_pct(78));
    lv_obj_set_style_bg_color(wifi_cfg_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_clear_flag(wifi_cfg_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(wifi_cfg_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(wifi_cfg_cont, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(wifi_cfg_cont, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(wifi_cfg_cont, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(wifi_cfg_cont, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(wifi_cfg_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifi_cfg_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_align(wifi_cfg_cont, LV_ALIGN_BOTTOM_MID, 0, -4);

    wifi_cfg_ssid_lab = lv_label_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_ssid_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_cfg_ssid_lab, FONT_BOLD_MONO_SIZE_16, LV_PART_MAIN);
    lv_label_set_text(wifi_cfg_ssid_lab, "AP: Mesh-...");

    wifi_cfg_pwd_lab = lv_label_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_pwd_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_cfg_pwd_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_text(wifi_cfg_pwd_lab, "Web: http://192.168.4.1");

    wifi_cfg_status_lab = lv_label_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_status_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_cfg_status_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_long_mode(wifi_cfg_status_lab, LV_LABEL_LONG_DOT);
    lv_label_set_text(wifi_cfg_status_lab, "Phone join AP, open Web");

    wifi_cfg_ip_lab = lv_label_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_ip_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_cfg_ip_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_text(wifi_cfg_ip_lab, "Clients: 0");

    wifi_cfg_rssi_lab = lv_label_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_rssi_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_cfg_rssi_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_text(wifi_cfg_rssi_lab, "RSSI: --");

    wifi_cfg_disc_btn = lv_btn_create(wifi_cfg_cont);
    lv_obj_set_width(wifi_cfg_disc_btn, lv_pct(100));
    lv_obj_set_height(wifi_cfg_disc_btn, 48);
    lv_obj_set_ext_click_area(wifi_cfg_disc_btn, 16);
    lv_obj_set_style_radius(wifi_cfg_disc_btn, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifi_cfg_disc_btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(wifi_cfg_disc_btn, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(wifi_cfg_disc_btn, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(wifi_cfg_disc_btn, DECKPRO_COLOR_BG, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(wifi_cfg_disc_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(wifi_cfg_disc_btn, wifi_cfg_disconnect_btn_cb, LV_EVENT_CLICKED, NULL);
    wifi_cfg_disc_lab = lv_label_create(wifi_cfg_disc_btn);
    lv_obj_set_style_text_font(wifi_cfg_disc_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(wifi_cfg_disc_lab, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_label_set_text(wifi_cfg_disc_lab, "Disconnect WiFi");
    lv_obj_center(wifi_cfg_disc_lab);

    // back
    scr_back_btn_create(parent, "Wifi Config", scr4_1_btn_event_cb);
}

static void entry4_1(void) 
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    w20_wifi_ota_stop();
    if (ui_wifi_softap_is_up()) {
        ui_wifi_force_ap_teardown(ui_wifi_is_connected());
    }
#endif
    /* 配网任务 prepareWifiRadioForAp() 已负责关 BLE 与 AP_STA，勿再 ui_wifi_menu_power_on() 重复初始化 */
    memset(wifi_cfg_last_line, 0, sizeof(wifi_cfg_last_line));
    wifi_cfg_disc_cache[0] = '\0';
    wifi_cfg_last_async_ms = 0;
    wifi_cfg_cancel_epd_pause_timer();
    wifi_cfg_set_epd_paused(false);
    ui_deckpro_pause_aux_timers();
    ui_wifi_provisioning_set_ui_callback(wifi_cfg_on_prov_change);
    if (!wifi_cfg_status_timer) {
        wifi_cfg_status_timer = lv_timer_create(wifi_cfg_status_timer_cb, 8000, NULL);
        lv_timer_ready(wifi_cfg_status_timer);
    }
    wifi_cfg_update_status();

    if (wifi_cfg_deferred_timer) {
        lv_timer_del(wifi_cfg_deferred_timer);
        wifi_cfg_deferred_timer = NULL;
    }
    wifi_cfg_deferred_timer = lv_timer_create(wifi_cfg_deferred_start_cb, 50, NULL);
    lv_timer_set_repeat_count(wifi_cfg_deferred_timer, 1);
}
static void wifi_cfg_exit_reclaim_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    /* 等 SoftAP/门户停稳再清堆，避免与 provTask 抢锁 */
    (void)ui_wifi_provisioning_wait_stopped(2500);
    ui_wifi_provisioning_reclaim_heap();
    W20_WIFI_LOG("[W20UI] WIFI Config exit reclaim heap=%u\n", (unsigned)ESP.getFreeHeap());
}

static void exit4_1(void) {
    if (wifi_cfg_deferred_timer) {
        lv_timer_del(wifi_cfg_deferred_timer);
        wifi_cfg_deferred_timer = NULL;
    }
    wifi_cfg_cancel_epd_pause_timer();
    wifi_cfg_set_epd_paused(false);
    ui_wifi_provisioning_set_ui_callback(NULL);
    if (wifi_cfg_status_timer) {
        lv_timer_del(wifi_cfg_status_timer);
        wifi_cfg_status_timer = NULL;
    }
    /* 勿在 LVGL 线程同步 stop/force teardown（易与 provTask 抢 WiFi 锁导致卡死）。 */
    ui_wifi_provisioning_request_stop();
    wifi_cfg_disc_btn = NULL;
    wifi_cfg_disc_lab = NULL;
    wifi_cfg_disc_cache[0] = '\0';
    ui_deckpro_resume_aux_timers();
    /* 延后清堆：扫网 JSON / 门户缓冲 / 空闲 wifiProv */
    lv_timer_t *reclaim_t = lv_timer_create(wifi_cfg_exit_reclaim_cb, 80, NULL);
    lv_timer_set_repeat_count(reclaim_t, 1);
}
static void destroy4_1(void) { }

static scr_lifecycle_t screen4_1 = {
    .create = create4_1,
    .entry = entry4_1,
    .exit  = exit4_1,
    .destroy = destroy4_1,
};
#endif
// --------------------- screen 4.2 --------------------- Wifi Scan
#if 1
static lv_obj_t *scr4_2_cont;
static lv_obj_t *wifi_scan_lab;
static lv_timer_t *wifi_scan_timer = NULL;

static ui_wifi_scan_info_t wifi_info_list[UI_WIFI_SCAN_ITEM_MAX];
static bool s_wifi_scan_async_pending = false;

static void scr4_2_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void show_wifi_scan(void)
{
#define BUFF_LEN 400
    char buf[BUFF_LEN];
    int ret = 0, offs = 0;

    ret = lv_snprintf(buf + offs, BUFF_LEN, "       NAME      | RSSI\n");
    offs = offs + ret;
    ret = lv_snprintf(buf + offs, BUFF_LEN, "-----------------------\n");
    offs = offs + ret;

    bool any = false;
    for(int i = 0; i < UI_WIFI_SCAN_ITEM_MAX; i++) {
        if(strcmp(wifi_info_list[i].name, "") == 0 && wifi_info_list[i].rssi == 0)
        {
            break;
        }
        any = true;
        if(i == UI_WIFI_SCAN_ITEM_MAX - 1) {
            ret = lv_snprintf(buf + offs, BUFF_LEN, "%-16.16s | %4d", wifi_info_list[i].name, wifi_info_list[i].rssi);
            break;
        }

        ret = lv_snprintf(buf + offs, BUFF_LEN, "%-16.16s | %4d\n", wifi_info_list[i].name, wifi_info_list[i].rssi);
        offs = offs + ret;
    }
    if (!any) {
        lv_label_set_text(wifi_scan_lab, "No AP found\nPull back & retry");
    } else {
        lv_label_set_text(wifi_scan_lab, buf);
    }
#undef BUFF_LEN
}

static void wifi_scan_timer_event(lv_timer_t *t)
{
    if (s_wifi_scan_async_pending) {
        const int r = ui_wifi_scan_poll_info(wifi_info_list, UI_WIFI_SCAN_ITEM_MAX);
        if (r == -1) {
            /* 仍在扫：保持 Scanning 文案，继续短周期轮询 */
            return;
        }
        s_wifi_scan_async_pending = false;
        if (r < 0) {
            lv_label_set_text(wifi_scan_lab, "Scan failed\nRetrying...");
        } else {
            show_wifi_scan();
        }
        ui_disp_full_refr();
        /* 下次周期性再扫 */
        if (t) {
            lv_timer_set_period(t, 10000);
        }
        return;
    }

    /* 空闲到期：启动异步扫描；进页 prepare 已刷过 Scanning，此处勿再 full_refr */
    if (wifi_scan_lab) {
        const char *cur = lv_label_get_text(wifi_scan_lab);
        if (cur == nullptr || strstr(cur, "Scanning") == nullptr) {
            lv_label_set_text(wifi_scan_lab, "Scanning...\nPlease wait");
        }
    }
    if (ui_wifi_scan_start_async()) {
        s_wifi_scan_async_pending = true;
        if (t) {
            lv_timer_set_period(t, 400);
        }
    } else {
        lv_label_set_text(wifi_scan_lab, "Scan unavailable\n(OTA/AP active?)");
        ui_disp_full_refr();
        if (t) {
            lv_timer_set_period(t, 10000);
        }
    }
}

static void create4_2(lv_obj_t *parent) 
{
    scr4_2_cont = lv_obj_create(parent);
    lv_obj_set_size(scr4_2_cont, lv_pct(100), lv_pct(90));
    lv_obj_set_style_bg_color(scr4_2_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr4_2_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr4_2_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(scr4_2_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr4_2_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(scr4_2_cont, 13, LV_PART_MAIN);
    lv_obj_set_flex_flow(scr4_2_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr4_2_cont, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr4_2_cont, 5, LV_PART_MAIN);
    lv_obj_set_align(scr4_2_cont, LV_ALIGN_BOTTOM_MID);

    wifi_scan_lab = lv_label_create(scr4_2_cont);
    lv_obj_set_width(wifi_scan_lab, lv_pct(95));
    lv_obj_set_style_pad_all(wifi_scan_lab, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(wifi_scan_lab, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);   
    lv_obj_set_style_border_width(wifi_scan_lab, 0, LV_PART_MAIN);
    lv_label_set_long_mode(wifi_scan_lab, LV_LABEL_LONG_WRAP);
    lv_label_set_text(wifi_scan_lab, "Scanning...\nPlease wait");

    lv_obj_t *back4_label = scr_back_btn_create(parent, ("Wifi"), scr4_2_btn_event_cb);
}
static void entry4_2(void) 
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (ui_wifi_softap_is_up()) {
        ui_wifi_force_ap_teardown(true);
    }
#endif
    ui_wifi_menu_power_on();
    memset(wifi_info_list, 0, sizeof(wifi_info_list));
    s_wifi_scan_async_pending = false;
    if (wifi_scan_lab) {
        lv_label_set_text(wifi_scan_lab, "Scanning...\nPlease wait");
    }
    /* 先进页显示 Scanning；350ms 后再异步扫，避免点击卡在同步 scanNetworks */
    if (wifi_scan_timer) {
        lv_timer_del(wifi_scan_timer);
        wifi_scan_timer = NULL;
    }
    wifi_scan_timer = lv_timer_create(wifi_scan_timer_event, 350, NULL);
    ui_deckpro_resume_aux_timers();
}
static void exit4_2(void) {
    s_wifi_scan_async_pending = false;
    if(wifi_scan_timer) {
        lv_timer_del(wifi_scan_timer);
        wifi_scan_timer = NULL;
    }
}

static void destroy4_2(void) { }

static scr_lifecycle_t screen4_2 = {
    .create = create4_2,
    .entry = entry4_2,
    .exit  = exit4_2,
    .destroy = destroy4_2,
};
#endif

// --------------------- screen 4.3 --------------------- Browser WiFi OTA
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
static lv_obj_t *wifi_ota_hint_lab = NULL;
static lv_obj_t *wifi_ota_url_lab = NULL;
static lv_obj_t *wifi_ota_status_lab = NULL;
static lv_obj_t *wifi_ota_prog_lab = NULL;
static lv_timer_t *wifi_ota_timer = NULL;
static char wifi_ota_hint_cache[64] = {0};
static char wifi_ota_url_cache[72] = {0};
static char wifi_ota_status_cache[96] = {0};
static char wifi_ota_prog_cache[24] = {0};
static lv_timer_t *wifi_ota_deferred_timer = NULL;
static TaskHandle_t s_wifi_ota_start_task = nullptr;
static volatile bool s_wifi_ota_start_cancel = false;

static void wifi_ota_request_full_refresh(void)
{
    /* 监听/上传全程停墨：再 FULL 会锁触控、抢堆，浏览器也难连上 */
    if (w20_wifi_ota_is_running() || w20_wifi_ota_is_busy()) {
        return;
    }
    lv_obj_t *scr = lv_scr_act();
    if (scr != nullptr) {
        lv_obj_invalidate(scr);
    }
    ui_disp_full_refr();
}

static bool wifi_ota_set_label_if_changed(lv_obj_t *lab, char *cache, size_t cache_len, const char *text)
{
    if (!lab || !cache || !text) {
        return false;
    }
    if (strncmp(cache, text, cache_len) == 0) {
        return false;
    }
    strncpy(cache, text, cache_len - 1);
    cache[cache_len - 1] = '\0';
    lv_label_set_text(lab, text);
    return true;
}

static bool wifi_ota_refresh_ui(void)
{
    bool changed = false;
    char hint[64];
    char ip[24] = {0};
    const bool hasIp = ui_wifi_sta_has_valid_ip();
    if (hasIp) {
        ui_wifi_get_ip(ip, sizeof(ip));
        snprintf(hint, sizeof(hint), "Same WiFi as PC, open URL:");
    } else if (ui_wifi_user_hold_offline()) {
        snprintf(hint, sizeof(hint), "Disconnected. Rejoin in WIFI Config");
    } else {
        snprintf(hint, sizeof(hint), "No WiFi. Use WIFI Config");
    }
    changed |= wifi_ota_set_label_if_changed(wifi_ota_hint_lab, wifi_ota_hint_cache, sizeof(wifi_ota_hint_cache), hint);

    char url[72] = {0};
    if (hasIp) {
        /* 有 IP 就显示完整 Web 地址；监听是否就绪看 Status 行 */
        w20_wifi_ota_get_url(url, sizeof(url));
    } else if (ui_wifi_user_hold_offline()) {
        snprintf(url, sizeof(url), "Go WIFI Config, pick WiFi");
    } else if (s_wifi_ota_start_task != nullptr) {
        snprintf(url, sizeof(url), "Linking WiFi...");
    } else {
        const char *st = w20_wifi_ota_get_status_text();
        if (st && (strcmp(st, "Need WiFi first") == 0 || strcmp(st, "Close WIFI Config") == 0 ||
                   strcmp(st, "Cancelled") == 0 || strcmp(st, "Connecting WiFi...") == 0 ||
                   strcmp(st, "Use WIFI Config") == 0)) {
            snprintf(url, sizeof(url), "%s", st);
        } else {
            snprintf(url, sizeof(url), "Go WIFI Config, link router");
        }
    }
    changed |= wifi_ota_set_label_if_changed(wifi_ota_url_lab, wifi_ota_url_cache, sizeof(wifi_ota_url_cache), url);

    char statusLine[96] = {0};
    if (w20_wifi_ota_is_running() && hasIp) {
        snprintf(statusLine, sizeof(statusLine), "%s", w20_wifi_ota_get_status_text());
    } else if (s_wifi_ota_start_task != nullptr) {
        snprintf(statusLine, sizeof(statusLine), hasIp ? "Starting OTA server..." : "Connecting saved WiFi...");
    } else if (!hasIp) {
        snprintf(statusLine, sizeof(statusLine), "OTA needs WiFi first");
    } else {
        const char *st = w20_wifi_ota_get_status_text();
        if (st && st[0] && strcmp(st, "Idle") != 0 && strcmp(st, "Stopped") != 0) {
            snprintf(statusLine, sizeof(statusLine), "%s", st);
        } else {
            snprintf(statusLine, sizeof(statusLine), "OTA server starting...");
        }
    }
    changed |= wifi_ota_set_label_if_changed(wifi_ota_status_lab, wifi_ota_status_cache, sizeof(wifi_ota_status_cache),
                                             statusLine);

    char prog[24] = {0};
    const int pct = w20_wifi_ota_get_progress_pct();
    if (pct >= 0) {
        snprintf(prog, sizeof(prog), "Progress: %d%%", pct);
    } else {
        snprintf(prog, sizeof(prog), "Progress: --");
    }
    changed |= wifi_ota_set_label_if_changed(wifi_ota_prog_lab, wifi_ota_prog_cache, sizeof(wifi_ota_prog_cache), prog);
    return changed;
}

static void wifi_ota_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const bool busy = w20_wifi_ota_is_busy();
    const bool running = w20_wifi_ota_is_running();
    /* 文案有变才考虑刷墨；监听/上传中只改 RAM，不上墨 */
    if (wifi_ota_refresh_ui()) {
        if (!running && !busy) {
            wifi_ota_request_full_refresh();
        }
    }
    if (wifi_ota_timer) {
        if (busy) {
            lv_timer_set_period(wifi_ota_timer, 2000);
        } else if (running) {
            lv_timer_set_period(wifi_ota_timer, 15000);
        } else {
            lv_timer_set_period(wifi_ota_timer, 3000);
        }
    }
}

static void wifi_ota_start_done_async(void *user_data)
{
    (void)user_data;
    if (scr_mgr_top_id() != SCREEN4_3_ID || s_wifi_ota_start_cancel) {
        ui_deckpro_resume_aux_timers();
        return;
    }
    (void)wifi_ota_refresh_ui();
    if (w20_wifi_ota_is_running()) {
        /* 只刷一次 URL/Server ready，然后继续停墨直到离开 OTA 页 */
        factory_resume_flush_timer();
        lv_obj_t *scr = lv_scr_act();
        if (scr != nullptr) {
            lv_obj_invalidate(scr);
        }
        ui_disp_full_refr();
        w20_deckpro_epd_drain();
        factory_pause_flush_timer();
        if (wifi_ota_timer) {
            lv_timer_set_period(wifi_ota_timer, 15000);
            lv_timer_reset(wifi_ota_timer);
            lv_timer_resume(wifi_ota_timer);
        }
    } else {
        factory_resume_flush_timer();
        wifi_ota_request_full_refresh();
        if (wifi_ota_timer) {
            lv_timer_set_period(wifi_ota_timer, 800);
            lv_timer_reset(wifi_ota_timer);
            lv_timer_resume(wifi_ota_timer);
        }
    }
    ui_deckpro_resume_aux_timers();
}

static void wifi_ota_start_task(void *param)
{
    (void)param;
    W20_WIFI_LOGLN("[W20WifiOta] start task running");
    w20_deckpro_epd_drain();
    factory_pause_flush_timer();
    ui_wifi_prepare_for_ota();
    if (!s_wifi_ota_start_cancel) {
        w20_wifi_ota_start();
    } else {
        W20_WIFI_LOGLN("[W20WifiOta] start cancelled");
    }
    s_wifi_ota_start_task = nullptr;
    /* OTA 已跑则保持停墨；否则恢复刷屏 */
    if (!w20_wifi_ota_is_running()) {
        factory_resume_flush_timer();
    }
    lv_async_call(wifi_ota_start_done_async, NULL);
    vTaskDelete(nullptr);
}

/** 堆不够建任务时同步启动（STA 已连则很快）。 */
static void wifi_ota_start_inline(void)
{
    W20_WIFI_LOG("[W20WifiOta] inline start heap=%u\n", (unsigned)ESP.getFreeHeap());
    w20_deckpro_epd_drain();
    factory_pause_flush_timer();
    ui_wifi_prepare_for_ota();
    if (!s_wifi_ota_start_cancel) {
        w20_wifi_ota_start();
    }
    if (!w20_wifi_ota_is_running()) {
        factory_resume_flush_timer();
    }
    wifi_ota_start_done_async(NULL);
}

static void wifi_ota_deferred_start_cb(lv_timer_t *timer)
{
    lv_timer_del(timer);
    wifi_ota_deferred_timer = NULL;

    if (scr_mgr_top_id() != SCREEN4_3_ID) {
        W20_WIFI_LOG("[W20WifiOta] deferred skip top_id=%d\n", scr_mgr_top_id());
        ui_deckpro_resume_aux_timers();
        return;
    }

    if (s_wifi_ota_start_task != nullptr) {
        W20_WIFI_LOGLN("[W20WifiOta] deferred: prior start still running, retry soon");
        s_wifi_ota_start_cancel = true;
        w20_wifi_ota_abort_start();
        wifi_ota_deferred_timer = lv_timer_create(wifi_ota_deferred_start_cb, 400, NULL);
        lv_timer_set_repeat_count(wifi_ota_deferred_timer, 1);
        return;
    }

    s_wifi_ota_start_cancel = false;
    /* 配网后堆常 <12KB；8192 栈必败。降栈重试，仍败则 inline，禁止死循环。 */
    static const uint32_t kStacks[] = {6144, 4096, 3072};
    static uint8_t s_createFailRounds = 0;
    BaseType_t created = pdFAIL;
    for (unsigned i = 0; i < sizeof(kStacks) / sizeof(kStacks[0]); i++) {
        s_wifi_ota_start_task = nullptr;
        created = xTaskCreate(wifi_ota_start_task, "wifiOtaSt", kStacks[i], nullptr, 1, &s_wifi_ota_start_task);
        if (created == pdPASS) {
            W20_WIFI_LOG("[W20WifiOta] wifiOtaSt ok stack=%u heap=%u\n", (unsigned)kStacks[i],
                          (unsigned)ESP.getFreeHeap());
            s_createFailRounds = 0;
            wifi_ota_refresh_ui();
            return;
        }
        s_wifi_ota_start_task = nullptr;
        Serial.printf("[W20WifiOta] wifiOtaSt xTaskCreate fail stack=%u heap=%u\n", (unsigned)kStacks[i],
                      (unsigned)ESP.getFreeHeap());
        delay(10);
    }

    s_createFailRounds++;
    if (s_createFailRounds >= 2) {
        Serial.println("[W20WifiOta] wifiOtaSt create failed — inline fallback");
        s_createFailRounds = 0;
        wifi_ota_start_inline();
        return;
    }

    Serial.println("[W20WifiOta] wifiOtaSt create failed — retry once then inline");
    wifi_ota_refresh_ui();
    wifi_ota_deferred_timer = lv_timer_create(wifi_ota_deferred_start_cb, 600, NULL);
    lv_timer_set_repeat_count(wifi_ota_deferred_timer, 1);
}

static void scr4_3_btn_event_cb(lv_event_t *e)
{
    if (e->code == LV_EVENT_CLICKED) {
        if (wifi_ota_timer) {
            lv_timer_pause(wifi_ota_timer);
        }
        scr_mgr_pop(false);
    }
}

static void create4_3(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    /* 接近全宽 + 小边距，保证 http://x.x.x.x:8080/ota 单行放下 */
    lv_obj_set_size(cont, lv_pct(100), lv_pct(76));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_right(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(cont, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 6, LV_PART_MAIN);

    wifi_ota_hint_lab = lv_label_create(cont);
    lv_obj_set_width(wifi_ota_hint_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_ota_hint_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_long_mode(wifi_ota_hint_lab, LV_LABEL_LONG_WRAP);
    lv_label_set_text(wifi_ota_hint_lab, "No WiFi. Use WIFI Config");

    wifi_ota_url_lab = lv_label_create(cont);
    lv_obj_set_width(wifi_ota_url_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_ota_url_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    /* 超宽自动换行，避免裁切或画出显示区 */
    lv_label_set_long_mode(wifi_ota_url_lab, LV_LABEL_LONG_WRAP);
    lv_label_set_text(wifi_ota_url_lab, "Go WIFI Config, link router");

    wifi_ota_status_lab = lv_label_create(cont);
    lv_obj_set_width(wifi_ota_status_lab, lv_pct(100));
    lv_obj_set_style_text_font(wifi_ota_status_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_long_mode(wifi_ota_status_lab, LV_LABEL_LONG_WRAP);
    lv_label_set_text(wifi_ota_status_lab, "OTA needs WiFi first");

    wifi_ota_prog_lab = lv_label_create(cont);
    lv_obj_set_style_text_font(wifi_ota_prog_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_text(wifi_ota_prog_lab, "Progress: --");

    scr_back_btn_create(parent, "WiFi OTA", scr4_3_btn_event_cb);
}

static void entry4_3(void)
{
    W20_WIFI_LOG("[W20UI] enter WiFi OTA page heap=%u\n", (unsigned)ESP.getFreeHeap());
    /* 进页立刻停墨，给 xTaskCreate / HTTP 腾堆与 CPU */
    factory_pause_flush_timer();
    ui_wifi_provisioning_request_stop();
    memset(wifi_ota_hint_cache, 0, sizeof(wifi_ota_hint_cache));
    memset(wifi_ota_url_cache, 0, sizeof(wifi_ota_url_cache));
    memset(wifi_ota_status_cache, 0, sizeof(wifi_ota_status_cache));
    memset(wifi_ota_prog_cache, 0, sizeof(wifi_ota_prog_cache));
    wifi_ota_refresh_ui();
    if (!wifi_ota_timer) {
        wifi_ota_timer = lv_timer_create(wifi_ota_timer_cb, 3000, NULL);
    } else {
        lv_timer_set_period(wifi_ota_timer, 3000);
    }

    if (wifi_ota_deferred_timer) {
        lv_timer_del(wifi_ota_deferred_timer);
        wifi_ota_deferred_timer = NULL;
    }
    /* 堆紧或刚离开 SoftAP：多等一会再启动，让 STA/堆稳定 */
    const uint32_t heap = ESP.getFreeHeap();
    const uint32_t defer_ms = (heap < 40000u) ? 2800u : 1800u;
    wifi_ota_deferred_timer = lv_timer_create(wifi_ota_deferred_start_cb, defer_ms, NULL);
    lv_timer_set_repeat_count(wifi_ota_deferred_timer, 1);
}

static void exit4_3(void)
{
    s_wifi_ota_start_cancel = true;
    w20_wifi_ota_abort_start();
    if (wifi_ota_deferred_timer) {
        lv_timer_del(wifi_ota_deferred_timer);
        wifi_ota_deferred_timer = NULL;
        ui_deckpro_resume_aux_timers();
    }
    /* 等后台重连任务退出，避免下次进 OTA 被旧任务挡住 */
    const unsigned long t0 = millis();
    while (s_wifi_ota_start_task != nullptr && (millis() - t0) < 2500) {
        delay(20);
    }
    w20_wifi_ota_stop(); /* 内含 reclaim_heap */
    factory_resume_flush_timer();
    W20_WIFI_LOG("[W20UI] WiFi OTA exit heap=%u\n", (unsigned)ESP.getFreeHeap());
    if (wifi_ota_timer) {
        lv_timer_del(wifi_ota_timer);
        wifi_ota_timer = NULL;
    }
    wifi_ota_hint_lab = NULL;
    wifi_ota_url_lab = NULL;
    wifi_ota_status_lab = NULL;
    wifi_ota_prog_lab = NULL;
}

static void destroy4_3(void) {}

static scr_lifecycle_t screen4_3 = {
    .create = create4_3,
    .entry = entry4_3,
    .exit = exit4_3,
    .destroy = destroy4_3,
};
#endif

//************************************[ screen 5 ]****************************************** Test
#if 1
static lv_obj_t *test_list;
static lv_timer_t *test_refresh_timer = NULL;
static int test_num = 0;
static int test_page_num = 0;
static int test_curr_page = 0;

static ui_test_handle test_handle_list[] = {
    { .name="INK_SCREEN", .peri_id=E_PERI_INK_SCREEN , .cb=ui_test_get },
    { .name="Lora (LR2021)", .peri_id=E_PERI_LORA    , .cb=ui_test_get },
    { .name="GPS (L76K)", .peri_id=E_PERI_GPS        , .cb=ui_test_get },
    { .name="Touch (U253)", .peri_id=E_PERI_TOUCH    , .cb=ui_test_get },
    { .name="IP5356",     .peri_id=E_PERI_IP5356     , .cb=ui_test_get },
    { .name="CW2217",     .peri_id=E_PERI_CW2217     , .cb=ui_test_get },
};

static void test_item_create(int curr_apge);
static bool test_refresh_status_labels(void);

static void scr5_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void test_item_create(int curr_apge)
{
    int start = (curr_apge * SETTING_PAGE_MAX_ITEM);
    int end = start + SETTING_PAGE_MAX_ITEM;
    if(end > test_num) end = test_num;

    for(int i = start; i < end; i++) {
        ui_test_handle *h = &test_handle_list[i];
        h->obj = lv_list_add_btn(test_list, NULL, h->name);
        h->st = lv_label_create(h->obj);
        lv_obj_set_style_text_font(h->st, FONT_BOLD_SIZE_15, LV_PART_MAIN);
        lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_label_set_text_fmt(h->st, "%s", (h->cb(h->peri_id) ? "PASS" : "----"));
        /* Test 自有行高（旧 35），不跟 Setting 的 40 */
        lv_obj_set_height(h->obj, TEST_ITEM_BTN_H);
        lv_obj_set_style_min_height(h->obj, TEST_ITEM_BTN_H, LV_PART_MAIN);
        lv_obj_set_style_max_height(h->obj, TEST_ITEM_BTN_H, LV_PART_MAIN);
        lv_obj_set_style_text_font(h->obj, FONT_BOLD_SIZE_15, LV_PART_MAIN);
        lv_obj_set_style_bg_color(h->obj, DECKPRO_COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_text_color(h->obj, DECKPRO_COLOR_FG, LV_PART_MAIN);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_outline_width(h->obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(h->obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(h->obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(h->obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(h->obj, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_right(h->obj, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(h->obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(h->obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(h->obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(h->obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_ext_click_area(h->obj, TEST_ITEM_CLICK_EXT);
        for (uint32_t ci = 0; ci < lv_obj_get_child_cnt(h->obj); ci++) {
            lv_obj_t *ch = lv_obj_get_child(h->obj, ci);
            if (lv_obj_check_type(ch, &lv_label_class)) {
                lv_obj_set_style_pad_top(ch, 0, LV_PART_MAIN);
                lv_obj_set_style_pad_bottom(ch, 0, LV_PART_MAIN);
                lv_obj_clear_flag(ch, LV_OBJ_FLAG_CLICKABLE);
                if (ch != h->st) {
                    lv_obj_set_style_text_align(ch, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
                }
            }
        }
        lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_add_flag(h->st, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(h->st, LV_OBJ_FLAG_CLICKABLE);
    }
}

/** 更新 PASS/---- 文案；仅当有变化时返回 true（避免无变化仍触发墨水刷新）。 */
static bool test_refresh_status_labels(void)
{
    bool changed = false;
    int start = (test_curr_page * SETTING_PAGE_MAX_ITEM);
    int end = start + SETTING_PAGE_MAX_ITEM;
    if (end > test_num) end = test_num;
    for (int i = start; i < end; i++) {
        ui_test_handle *h = &test_handle_list[i];
        if (!h->st) {
            continue;
        }
        const char *want = h->cb(h->peri_id) ? "PASS" : "----";
        const char *cur = lv_label_get_text(h->st);
        if (cur == nullptr || strcmp(cur, want) != 0) {
            lv_label_set_text(h->st, want);
            changed = true;
        }
    }
    return changed;
}

static void test_refresh_timer_event(lv_timer_t *t)
{
    (void)t;
    if (test_refresh_status_labels()) {
        ui_disp_full_refr();
    }
}

static void create5(lv_obj_t *parent) 
{
    test_list = lv_list_create(parent);
    lv_obj_set_size(test_list, LV_HOR_RES, lv_pct(93));
    lv_obj_align(test_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(test_list, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_pad_top(test_list, TEST_ITEM_PAD_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_row(test_list, TEST_ITEM_PAD_ROW, LV_PART_MAIN);
    lv_obj_set_style_radius(test_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(test_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(test_list, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(test_list, 0, LV_PART_MAIN);

    test_num = sizeof(test_handle_list) / sizeof(test_handle_list[0]);
    test_page_num = test_num / SETTING_PAGE_MAX_ITEM;
    test_item_create(test_curr_page);
    lv_obj_update_layout(test_list);

    lv_obj_t *back5_label = scr_back_btn_create(parent, ("Test"), scr5_btn_event_cb);
}
static void entry5(void) 
{
    (void)test_refresh_status_labels();
    /* 每 5s 探测一次；仅状态变化时刷墨 */
    test_refresh_timer = lv_timer_create(test_refresh_timer_event, 5000, NULL);
}
static void exit5(void) {
    if (test_refresh_timer) {
        lv_timer_del(test_refresh_timer);
        test_refresh_timer = NULL;
    }
}
static void destroy5(void) { }

static scr_lifecycle_t screen5 = {
    .create = create5,
    .entry = entry5,
    .exit  = exit5,
    .destroy = destroy5,
};
#endif
//************************************[ screen 6 ]****************************************** Battery (IP5356 可读寄存器直显)
#if 1

#define SCR6_LINE_MAX 23
/** Battery 页 Cap Full / Cap Remain 展示用；不改 IP5356_BATTERY_DESIGN_MAH 等内部标定。 */
#define SCR6_CAP_FULL_DISPLAY_MAH 9000u

/** 仅文案变化时写 label，避免 Offline 等静态态每 5s 触发墨水 PART。 */
static bool scr6_battery_set_line_if_changed(lv_obj_t *label, const char *str1, const char *str2)
{
    if (!label) {
        return false;
    }
    char line[SCR6_LINE_MAX + 8];
    int w2 = strlen(str2);
    int w1 = SCR6_LINE_MAX - w2;
    lv_snprintf(line, sizeof(line), "%-*s%-*s", w1, str1, w2, str2);
    const char *cur = lv_label_get_text(label);
    if (cur != nullptr && strcmp(cur, line) == 0) {
        return false;
    }
    lv_label_set_text(label, line);
    return true;
}

static lv_timer_t *scr6_batt_timer = NULL;

static lv_obj_t *scr6_create_label(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, lv_pct(90));
    lv_obj_set_style_text_font(label, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_border_side(label, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    return label;
}

/** 更新各行；返回是否有任何可见文案变化。 */
static bool scr6_battery_update(void)
{
    char buf[SCR6_LINE_MAX];
    bool changed = false;

    ui_battery_power_line(buf, SCR6_LINE_MAX);
    changed |= scr6_battery_set_line_if_changed(label_list[0], "VIN / VBUS:", buf);

    changed |= scr6_battery_set_line_if_changed(label_list[1], "Charge ST:", ui_battery_charge_state_label());

    lv_snprintf(buf, SCR6_LINE_MAX, "0x%04x", ui_battery_status_raw());
    changed |= scr6_battery_set_line_if_changed(label_list[2], "Status C4|E9:", buf);

    lv_snprintf(buf, SCR6_LINE_MAX, "%umV", ui_battery_voltage_mv());
    changed |= scr6_battery_set_line_if_changed(label_list[3], "VBAT:", buf);

    if (ui_battery_gauge_ok()) {
        lv_snprintf(buf, SCR6_LINE_MAX, "%dmA", (int)ui_battery_current_ma());
    } else {
        lv_snprintf(buf, SCR6_LINE_MAX, "N/A");
    }
    changed |= scr6_battery_set_line_if_changed(label_list[4], "Current:", buf);

    {
        const int16_t ntcC = ui_battery_ntc_temp_c();
        if (ntcC > -999) {
            lv_snprintf(buf, SCR6_LINE_MAX, "%d°C", (int)ntcC);
        } else {
            lv_snprintf(buf, SCR6_LINE_MAX, "N/A");
        }
    }
    changed |= scr6_battery_set_line_if_changed(label_list[5], "Temperature:", buf);

    {
        /* 与 Cap Full 展示一致：按 SCR6_CAP_FULL_DISPLAY_MAH × SOC%，勿用内部 10000 标定。 */
        const unsigned remain =
            (unsigned)((SCR6_CAP_FULL_DISPLAY_MAH * (uint32_t)ui_battery_percent()) / 100U);
        lv_snprintf(buf, SCR6_LINE_MAX, "%umAh", remain);
    }
    changed |= scr6_battery_set_line_if_changed(label_list[6], "Cap Remain:", buf);

    lv_snprintf(buf, SCR6_LINE_MAX, "%umAh", (unsigned)SCR6_CAP_FULL_DISPLAY_MAH);
    changed |= scr6_battery_set_line_if_changed(label_list[7], "Cap Full:", buf);

    if (ui_battery_soc_is_estimated()) {
        lv_snprintf(buf, SCR6_LINE_MAX, "%u%% (est.)", ui_battery_percent());
    } else {
        lv_snprintf(buf, SCR6_LINE_MAX, "%u%%", ui_battery_percent());
    }
    changed |= scr6_battery_set_line_if_changed(label_list[8], "SOC:", buf);

    lv_snprintf(buf, SCR6_LINE_MAX, "%u%%", ui_battery_health_percent());
    changed |= scr6_battery_set_line_if_changed(label_list[9], "Health:", buf);

    return changed;
}

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
static int8_t s_scr6_epd_track_vbus = -1;
static int8_t s_scr6_epd_track_charging = -1;
static int8_t s_scr6_epd_track_finish = -1;

static void scr6_battery_reset_charge_epd_track(void)
{
    s_scr6_epd_track_vbus = -1;
    s_scr6_epd_track_charging = -1;
    s_scr6_epd_track_finish = -1;
}

static bool scr6_battery_charge_epd_changed(void)
{
    const int8_t vbus = ui_battery_vbus_in() ? 1 : 0;
    const int8_t chg = ui_battery_charging() ? 1 : 0;
    const int8_t fin = ui_battery_charge_complete() ? 1 : 0;

    if (s_scr6_epd_track_vbus < 0) {
        s_scr6_epd_track_vbus = vbus;
        s_scr6_epd_track_charging = chg;
        s_scr6_epd_track_finish = fin;
        return false;
    }

    const bool changed =
        (vbus != s_scr6_epd_track_vbus || chg != s_scr6_epd_track_charging || fin != s_scr6_epd_track_finish);
    s_scr6_epd_track_vbus = vbus;
    s_scr6_epd_track_charging = chg;
    s_scr6_epd_track_finish = fin;
    return changed;
}
#endif

static void scr6_batt_timer_event(lv_timer_t *t)
{
    (void)t;
    /* 每 5s 探测；Offline/数值不变则不刷墨 */
    if (scr6_battery_update()) {
        ui_disp_full_refr();
    }
}

static void scr6_btn_event_cb(lv_event_t *e)
{
    if (e->code == LV_EVENT_CLICKED) {
        scr_mgr_pop(false);
    }
}

static void create6(lv_obj_t *parent)
{
    lv_obj_t *scr6_cont = lv_obj_create(parent);
    lv_obj_set_size(scr6_cont, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_color(scr6_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr6_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr6_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(scr6_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr6_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(scr6_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr6_cont, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr6_cont, 0, LV_PART_MAIN);
    lv_obj_set_align(scr6_cont, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_flex_flow(scr6_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr6_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < (int)(sizeof(label_list) / sizeof(label_list[0])); i++) {
        label_list[i] = scr6_create_label(scr6_cont);
    }
    scr_back_btn_create(parent, "Battery", scr6_btn_event_cb);
}

static void entry6(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    scr6_battery_reset_charge_epd_track();
#endif
    (void)scr6_battery_update();
    /* 探测间隔 5s；刷墨仅在文案变化时（见 timer） */
    scr6_batt_timer = lv_timer_create(scr6_batt_timer_event, 5000, NULL);
}

static void exit6(void)
{
    if (scr6_batt_timer) {
        lv_timer_del(scr6_batt_timer);
        scr6_batt_timer = NULL;
    }
}

static void destroy6(void) {}

static scr_lifecycle_t screen6 = {
    .create = create6,
    .entry = entry6,
    .exit = exit6,
    .destroy = destroy6,
};
#undef SCR6_LINE_MAX
#endif
//************************************[ screen 7 ]****************************************** Other
#if 1
static lv_obj_t *scr7_cont;
static lv_obj_t *input_touch;
static lv_timer_t *input_timer;

static void scr7_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void input_timer_event(lv_timer_t *t)
{
    int touch_x, touch_y;

    int ret = ui_input_get_touch_coord(&touch_x, &touch_y);

    if(ret > 0)
    {
        lv_label_set_text_fmt(input_touch,  "Touch: x: %03d | y: %03d", touch_x, touch_y);
    }
}

static void create7(lv_obj_t *parent) 
{
    scr7_cont = lv_obj_create(parent);
    lv_obj_set_size(scr7_cont, lv_pct(100), lv_pct(88));
    lv_obj_set_style_bg_color(scr7_cont, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr7_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr7_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(scr7_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr7_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(scr7_cont, 13, LV_PART_MAIN);
    lv_obj_set_flex_flow(scr7_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr7_cont, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(scr7_cont, 5, LV_PART_MAIN);
    lv_obj_set_align(scr7_cont, LV_ALIGN_BOTTOM_MID);

    input_touch = lv_label_create(scr7_cont);
    // lv_obj_set_height(input_touch, 90);
    lv_obj_set_width(input_touch, lv_pct(95));
    lv_obj_set_style_pad_all(input_touch, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(input_touch, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);
    // lv_obj_set_style_border_width(input_touch, 1, LV_PART_MAIN);
    lv_label_set_long_mode(input_touch, LV_LABEL_LONG_WRAP);
    lv_label_set_text(input_touch,  "Touch: x:     | y:    ");
    lv_obj_move_foreground(input_touch);

    lv_obj_t *back7_label = scr_back_btn_create(parent, ("Input"), scr7_btn_event_cb);
}
static void entry7(void) 
{
    lv_label_set_text(input_touch,  "Touch: x:     | y:    ");
    input_timer = lv_timer_create(input_timer_event, 100, NULL);
}
static void exit7(void) {
    if(input_timer)
    {
        lv_timer_del(input_timer);
        input_timer = NULL;
    }
    lv_label_set_text(input_touch,  "Touch: x:     | y:    ");
}
static void destroy7(void) { }

static scr_lifecycle_t screen7 = {
    .create = create7,
    .entry = entry7,
    .exit  = exit7,
    .destroy = destroy7,
};
#endif
//************************************[ screen 8 ]****************************************** A7682E
// --------------------- screen 8 --------------------- A7682E
#if 1
static lv_obj_t *a7682_list;
static lv_obj_t *a7682_page;
static int a7682_num = 0;
static int a7682_page_num = 0;
static int a7682_curr_page = 0;

bool ui_a7682_call_test(const char *param)
{
    scr_push_subscreen(SCREEN8_1_ID);
    return true;
}

bool ui_a7682_at_test(const char *param)
{
    scr_push_subscreen(SCREEN8_2_ID);
    return true;
}

static ui_a7682_handle a7682_handle_list[] = 
{
    {"A7682 Audio", NULL, NULL, ui_a7682_at_cb},
    {"Call test", NULL, NULL, ui_a7682_call_test},
    {"AT test", NULL, NULL, ui_a7682_at_test},
};

static void a7682_item_create(int curr_apge);

static void a7682_scr_event(lv_event_t *e)
{
    lv_obj_t *tgt = (lv_obj_t *)e->target;
    ui_a7682_handle *h = (ui_a7682_handle *)e->user_data;

    if(e->code == LV_EVENT_CLICKED) {
        if(h->cb)
            h->cb(h->name);
    }
}

static void a7682_item_create(int curr_apge)
{
    int start = (curr_apge * SETTING_PAGE_MAX_ITEM);
    int end = start + SETTING_PAGE_MAX_ITEM;
    if(end > a7682_num) end = a7682_num;

    for(int i = start; i < end; i++) {
        ui_a7682_handle *h = &a7682_handle_list[i];
        h->obj = lv_list_add_btn(a7682_list, NULL, h->name);
        lv_obj_set_height(h->obj, 28);
        // h->st = lv_label_create(h->obj);
        // lv_obj_set_style_text_font(h->st, FONT_BOLD_SIZE_15, LV_PART_MAIN);
        // lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
        // lv_label_set_text_fmt(h->st, "%s", (h->get_cb() ? "ON" : "OFF"));
        // style
        lv_obj_set_style_text_font(h->obj, FONT_BOLD_SIZE_14, LV_PART_MAIN);
        lv_obj_set_style_bg_color(h->obj, DECKPRO_COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_text_color(h->obj, DECKPRO_COLOR_FG, LV_PART_MAIN);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_outline_width(h->obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(h->obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(h->obj, a7682_scr_event, LV_EVENT_CLICKED, (void *)h);
    }
}

static void a7682_page_switch_cb(lv_event_t *e)
{
    char opt = (int)e->user_data;
    
    if(a7682_num < SETTING_PAGE_MAX_ITEM) return;

    int child_cnt = lv_obj_get_child_cnt(a7682_list);
    
    for(int i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(a7682_list, 0);
        if(child)
            lv_obj_del(child);
    }

    if(opt == 'p')
    {
        a7682_curr_page = (a7682_curr_page < a7682_page_num) ? a7682_curr_page + 1 : 0;
    }
    else if(opt == 'n')
    {
        a7682_curr_page = (a7682_curr_page > 0) ? a7682_curr_page - 1 : a7682_page_num;
    }

    a7682_item_create(a7682_curr_page);
    lv_label_set_text_fmt(a7682_page, "%d / %d", a7682_curr_page, a7682_page_num);
}

static void scr8_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void create8(lv_obj_t *parent) 
{
    a7682_list = lv_list_create(parent);
    lv_obj_set_size(a7682_list, LV_HOR_RES, lv_pct(88));
    lv_obj_align(a7682_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(a7682_list, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_pad_top(a7682_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(a7682_list, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(a7682_list, 0, LV_PART_MAIN);
    // lv_obj_set_style_outline_pad(a7682_list, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(a7682_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(a7682_list, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(a7682_list, 0, LV_PART_MAIN);

    a7682_num = sizeof(a7682_handle_list) / sizeof(a7682_handle_list[0]);
    a7682_page_num = a7682_num / SETTING_PAGE_MAX_ITEM;
    a7682_item_create(a7682_curr_page);

    lv_obj_t * ui_Button2 = lv_btn_create(parent);
    lv_obj_set_width(ui_Button2, 71);
    lv_obj_set_height(ui_Button2, 40);
    lv_obj_set_x(ui_Button2, -70);
    lv_obj_set_y(ui_Button2, 130);
    lv_obj_set_align(ui_Button2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Button2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_spread(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui_Button2, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Label1 = lv_label_create(ui_Button2);
    lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label1, "Back");
    lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Button14 = lv_btn_create(parent);
    lv_obj_set_width(ui_Button14, 71);
    lv_obj_set_height(ui_Button14, 40);
    lv_obj_set_x(ui_Button14, 70);
    lv_obj_set_y(ui_Button14, 130);
    lv_obj_set_align(ui_Button14, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button14, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button14, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button14, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button14, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_spread(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui_Button14, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Label15 = lv_label_create(ui_Button14);
    lv_obj_set_width(ui_Label15, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label15, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label15, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label15, "Next");
    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Button2, a7682_page_switch_cb, LV_EVENT_CLICKED, (void*)'n');
    lv_obj_add_event_cb(ui_Button14, a7682_page_switch_cb, LV_EVENT_CLICKED, (void*)'p');

    a7682_page = lv_label_create(parent);
    lv_obj_set_width(a7682_page, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(a7682_page, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(a7682_page, LV_ALIGN_BOTTOM_MID, 0, -23);
    lv_label_set_text_fmt(a7682_page, "%d / %d", a7682_curr_page, a7682_page_num);
    lv_obj_set_style_text_color(a7682_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(a7682_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *back8_label = scr_back_btn_create(parent, ("A7682E"), scr8_btn_event_cb);
}
static void entry8(void) 
{
}
static void exit8(void) {
}
static void destroy8(void) { }

static scr_lifecycle_t screen8 = {
    .create = create8,
    .entry = entry8,
    .exit  = exit8,
    .destroy = destroy8,
};
#endif
// --------------------- screen 8.1 --------------------- Call test
#if 1
static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * ta =  (lv_obj_t *)lv_event_get_user_data(e);

    if(code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        const char * txt = lv_btnmatrix_get_btn_text(obj, id);
        int len = strlen(txt);
 
        if(!strcmp(txt, LV_SYMBOL_CALL)) {
            ui_a7682_call(lv_textarea_get_text(ta));
        } else if(!strcmp(txt, "Hang up"))
        {
            ui_a7682_hang_up();
        } else if(!strcmp(txt, LV_SYMBOL_BACKSPACE))
        {
            lv_textarea_del_char(ta);
        }else{
            lv_textarea_add_text(ta, txt);
        }
    }
}

static const char * btnm_map[] = {  "1", "2", "3", "\n",
                                    "4", "5", "6", "\n",
                                    "7", "8", "9", "\n",
                                    "*", "0", "#", "\n",
                                    LV_SYMBOL_CALL, "Hang up", LV_SYMBOL_BACKSPACE,""
                                 };


static void scr8_1_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void create8_1(lv_obj_t *parent) 
{
    lv_obj_t * ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_obj_set_width(ta, lv_pct(98));
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, lv_pct(20));
    lv_obj_set_style_text_font(ta, &Font_Mono_Bold_20, LV_PART_MAIN);
    // lv_obj_add_state(ta, LV_STATE_FOCUSED); /*To be sure the cursor is visible*/
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_letter_space(ta, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ta, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * btnm1 = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(btnm1, btnm_map);
    lv_obj_set_size(btnm1, lv_pct(100)-2, lv_pct(60));
    lv_obj_set_style_border_width(btnm1, 0, 0);
    // lv_btnmatrix_set_btn_width(btnm1, 10, 2);        /*Make "Action1" twice as wide as "Action2"*/
    // lv_btnmatrix_set_btn_ctrl(btnm1, 10, LV_BTNMATRIX_CTRL_CHECKABLE);
    // lv_btnmatrix_set_btn_ctrl(btnm1, 11, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_align(btnm1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(btnm1, event_handler, LV_EVENT_VALUE_CHANGED, ta);
    
    lv_obj_t *back8_1_label = scr_back_btn_create(parent, ("Call"), scr8_1_btn_event_cb);
}
static void entry8_1(void) 
{
    ui_a7682_loop_resume();
}
static void exit8_1(void) {
    ui_a7682_loop_suspend();
}
static void destroy8_1(void) { }

static scr_lifecycle_t screen8_1 = {
    .create = create8_1,
    .entry = entry8_1,
    .exit  = exit8_1,
    .destroy = destroy8_1,
};
#endif
// --------------------- screen 8.2 --------------------- AT test
#if 1
static void scr8_2_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void create8_2(lv_obj_t *parent) 
{
    lv_obj_t *lab = lv_label_create(parent);
    lv_obj_set_width(lab, lv_pct(95));
    lv_obj_set_style_text_font(lab, FONT_BOLD_SIZE_17, LV_PART_MAIN);
    lv_label_set_text(lab, "Open the serial port, set the baud rate to 115200, "
                            "and send the AT command of A7682E to test the function.");
    lv_obj_center(lab);
    
    lv_obj_t *back8_2_label = scr_back_btn_create(parent, ("AT test"), scr8_2_btn_event_cb);
}
static void entry8_2(void) 
{
    ui_a7682_loop_resume();
}
static void exit8_2(void) {
    ui_a7682_loop_suspend();
}
static void destroy8_2(void) { }

static scr_lifecycle_t screen8_2 = {
    .create = create8_2,
    .entry = entry8_2,
    .exit  = exit8_2,
    .destroy = destroy8_2,
};
#endif
//************************************[ screen 9 ]****************************************** Shutdown
#if 1
static lv_timer_t *shutdown_timer = NULL;
/** 0=等关机页 FULL 刷完；1=画面已可见，再 hold 一段时间后真正关机 */
static int s_shutdown_phase = 0;
static uint32_t s_shutdown_hold_since_ms = 0;
/** Deck 关机页已上屏：禁止 Mesh 松手 SHUTDOWN 再刷「Shutting Down...」盖住 logo。 */
static bool s_deck_shutdown_ui_active = false;
/** 低电自动关机：SCREEN9 在 logo 底部加「Low battery off」（手动 Shutdown / Boot 长按不加）。 */
static bool s_shutdown_low_batt_hint = false;

#ifndef W20_SHUTDOWN_LOGO_HOLD_MS
#define W20_SHUTDOWN_LOGO_HOLD_MS 2000u
#endif

static void scr9_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void shutdown_timer_event(lv_timer_t* t)
{
    if (s_shutdown_phase == 0) {
        /* 点击后 FULL 约 1.3s；旧逻辑从点击起 2s 就关机，画面大约只亮 0.7s */
        if (ui_deckpro_epd_flush_busy()) {
            return;
        }
        s_shutdown_phase = 1;
        s_shutdown_hold_since_ms = millis();
        W20_UI_LOG("shutdown logo visible, hold %ums", (unsigned)W20_SHUTDOWN_LOGO_HOLD_MS);
        return;
    }
    if ((uint32_t)(millis() - s_shutdown_hold_since_ms) < W20_SHUTDOWN_LOGO_HOLD_MS) {
        return;
    }
    ui_shutdown_on();
    lv_timer_del(t);
    shutdown_timer = NULL;
}

static void deckpro_logo_show(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, DECKPRO_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(parent, 0, 0);

    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, &img_start);
    lv_obj_set_style_border_width(img, 0, 0);
    lv_obj_set_style_pad_all(img, 0, 0);
    lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
}

static void create9(lv_obj_t *parent)
{
    /* 手动点 Shutdown：无论是否插着 USB / 充电中，都允许关机。
     * 早期版本在此处判定 ui_battery_vbus_in() 阻止充电中关机，与用户预期不符。 */
    deckpro_logo_show(parent);
    /* 低电：对齐 Sleep 版式 — img_start + 底部提示；仍走真关机（非软睡）。 */
    if (s_shutdown_low_batt_hint) {
        lv_obj_t *hint = lv_label_create(parent);
        lv_label_set_text(hint, "Low battery off");
        lv_obj_set_style_text_color(hint, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, FONT_BOLD_MONO_SIZE_19, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -28);
        s_shutdown_low_batt_hint = false;
    }
    s_shutdown_phase = 0;
    s_shutdown_hold_since_ms = 0;
    s_deck_shutdown_ui_active = true;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    /* 取消 Mesh ButtonThread 已排队的关机，避免松手后 Power::shutdown 盖屏 */
    shutdownAtMsec = 0;
#endif
    /* 200ms 轮询：先等刷墨完成，再 hold 2s，避免「感觉没显示就关机」 */
    shutdown_timer = lv_timer_create(shutdown_timer_event, 200, (void *)parent);
}
static void entry9(void) 
{
}
static void exit9(void) {
}
static void destroy9(void) { }

static scr_lifecycle_t screen9 = {
    .create = create9,
    .entry = entry9,
    .exit  = exit9,
    .destroy = destroy9,
};
#endif
//************************************[ screen 12 ]***************************************** boot logo
#if 1
static void boot_timer_event(lv_timer_t *t)
{
    scr_mgr_switch(SCREEN0_ID, false);
    menu_taskbar_refresh_all();
    lv_obj_t *scr = lv_scr_act();
    if (scr != nullptr) {
        lv_obj_invalidate(scr);
    }
    ui_disp_full_refr();
    lv_timer_del(t);
}

static void create12(lv_obj_t *parent)
{
    deckpro_logo_show(parent);
    lv_timer_create(boot_timer_event, 2000, NULL);
}

static void entry12(void)
{
    ui_disp_full_refr();
}

static void exit12(void)
{
    ui_disp_full_refr();
}

static void destroy12(void) { }

static scr_lifecycle_t screen12 = {
    .create = create12,
    .entry = entry12,
    .exit = exit12,
    .destroy = destroy12,
};
#endif
//************************************[ screen 10 ]***************************************** pcm5102
#if 1
static lv_obj_t *pcm5102_list;
static lv_obj_t *pcm5102_page;
static int pcm5102_num = 0;
static int pcm5102_page_num = 0;
static int pcm5102_curr_page = 0;

static ui_pcm5102_handle pcm5102_handle_list[] = 
{
    {"PCM5102 Audio", NULL, NULL, ui_pcm5102_cb},
};

static void pcm5102_item_create(int curr_apge);

static void pcm5102_scr_event(lv_event_t *e)
{
    lv_obj_t *tgt = (lv_obj_t *)e->target;
    ui_pcm5102_handle *h = (ui_pcm5102_handle *)e->user_data;

    if(e->code == LV_EVENT_CLICKED) {
        if(h->cb)
            h->cb(h->name);
    }
}

static void pcm5102_item_create(int curr_apge)
{
    int start = (curr_apge * SETTING_PAGE_MAX_ITEM);
    int end = start + SETTING_PAGE_MAX_ITEM;
    if(end > pcm5102_num) end = pcm5102_num;

    for(int i = start; i < end; i++) {
        ui_pcm5102_handle *h = &pcm5102_handle_list[i];
        h->obj = lv_list_add_btn(pcm5102_list, NULL, h->name);
        lv_obj_set_height(h->obj, 28);
        // h->st = lv_label_create(h->obj);
        // lv_obj_set_style_text_font(h->st, FONT_BOLD_SIZE_15, LV_PART_MAIN);
        // lv_obj_align(h->st, LV_ALIGN_RIGHT_MID, 0, 0);
        // lv_label_set_text_fmt(h->st, "%s", (h->get_cb() ? "ON" : "OFF"));
        // style
        lv_obj_set_style_text_font(h->obj, FONT_BOLD_SIZE_14, LV_PART_MAIN);
        lv_obj_set_style_bg_color(h->obj, DECKPRO_COLOR_BG, LV_PART_MAIN);
        lv_obj_set_style_text_color(h->obj, DECKPRO_COLOR_FG, LV_PART_MAIN);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(h->obj, 1, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_outline_width(h->obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(h->obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(h->obj, pcm5102_scr_event, LV_EVENT_CLICKED, (void *)h);
    }
}

static void pcm5102_page_switch_cb(lv_event_t *e)
{
    char opt = (int)e->user_data;
    
    if(pcm5102_num < SETTING_PAGE_MAX_ITEM) return;

    int child_cnt = lv_obj_get_child_cnt(pcm5102_list);
    
    for(int i = 0; i < child_cnt; i++)
    {
        lv_obj_t *child = lv_obj_get_child(pcm5102_list, 0);
        if(child)
            lv_obj_del(child);
    }

    if(opt == 'p')
    {
        pcm5102_curr_page = (pcm5102_curr_page < pcm5102_page_num) ? pcm5102_curr_page + 1 : 0;
    }
    else if(opt == 'n')
    {
        pcm5102_curr_page = (pcm5102_curr_page > 0) ? pcm5102_curr_page - 1 : pcm5102_page_num;
    }

    pcm5102_item_create(pcm5102_curr_page);
    lv_label_set_text_fmt(pcm5102_page, "%d / %d", pcm5102_curr_page, pcm5102_page_num);
}


static void scr10_btn_event_cb(lv_event_t * e)
{
    if(e->code == LV_EVENT_CLICKED){
        scr_mgr_pop(false);
    }
}

static void create10(lv_obj_t *parent) 
{
    pcm5102_list = lv_list_create(parent);
    lv_obj_set_size(pcm5102_list, LV_HOR_RES, lv_pct(88));
    lv_obj_align(pcm5102_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(pcm5102_list, DECKPRO_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_pad_top(pcm5102_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(pcm5102_list, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(pcm5102_list, 0, LV_PART_MAIN);
    // lv_obj_set_style_outline_pad(pcm5102_list, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(pcm5102_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(pcm5102_list, DECKPRO_COLOR_FG, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(pcm5102_list, 0, LV_PART_MAIN);

    pcm5102_num = sizeof(pcm5102_handle_list) / sizeof(pcm5102_handle_list[0]);
    pcm5102_page_num = pcm5102_num / SETTING_PAGE_MAX_ITEM;
    pcm5102_item_create(pcm5102_curr_page);

    lv_obj_t * ui_Button2 = lv_btn_create(parent);
    lv_obj_set_width(ui_Button2, 71);
    lv_obj_set_height(ui_Button2, 40);
    lv_obj_set_x(ui_Button2, -70);
    lv_obj_set_y(ui_Button2, 130);
    lv_obj_set_align(ui_Button2, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Button2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_spread(ui_Button2, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui_Button2, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Label1 = lv_label_create(ui_Button2);
    lv_obj_set_width(ui_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label1, "Back");
    lv_obj_set_style_text_color(ui_Label1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Button14 = lv_btn_create(parent);
    lv_obj_set_width(ui_Button14, 71);
    lv_obj_set_height(ui_Button14, 40);
    lv_obj_set_x(ui_Button14, 70);
    lv_obj_set_y(ui_Button14, 130);
    lv_obj_set_align(ui_Button14, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button14, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button14, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button14, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button14, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_spread(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_shadow_spread(ui_Button14, 0, LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_set_style_radius(ui_Button14, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * ui_Label15 = lv_label_create(ui_Button14);
    lv_obj_set_width(ui_Label15, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label15, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label15, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label15, "Next");
    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_Label15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Button2, pcm5102_page_switch_cb, LV_EVENT_CLICKED, (void*)'n');
    lv_obj_add_event_cb(ui_Button14, pcm5102_page_switch_cb, LV_EVENT_CLICKED, (void*)'p');

    pcm5102_page = lv_label_create(parent);
    lv_obj_set_width(pcm5102_page, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(pcm5102_page, LV_SIZE_CONTENT);    /// 1
    lv_obj_align(pcm5102_page, LV_ALIGN_BOTTOM_MID, 0, -23);
    lv_label_set_text_fmt(pcm5102_page, "%d / %d", pcm5102_curr_page, pcm5102_page_num);
    lv_obj_set_style_text_color(pcm5102_page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(pcm5102_page, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *back10_label = scr_back_btn_create(parent, ("PCM5102"), scr10_btn_event_cb);
}
static void entry10(void) 
{
}
static void exit10(void) 
{
    ui_pcm5102_stop();
}
static void destroy10(void) { }

static scr_lifecycle_t screen10 = {
    .create = create10,
    .entry = entry10,
    .exit  = exit10,
    .destroy = destroy10,
};
#endif
//************************************[ screen 11 ]****************************************** eBook
#if 1
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
void ui_deckpro_enter_board_sleep(void)
{
    ui_deckpro_pause_aux_timers();
    /* sleep_prompt 在 w20_deck_sleep_enter 统一刷；LVGL 仍停在菜单，唤醒 present_menu */
    w20_deck_sleep_request_dark();
    if (screen != nullptr) {
        screen->setOn(false);
    }
    if (!w20_deck_sleep_active()) {
        w20_deck_sleep_enter();
    }
    W20_UI_LOG("board_sleep enter page_curr=%d", page_curr);
}
#endif

/* ---------- eBook TOC (SCREEN11) + reader (SCREEN11_1) ---------- */
static lv_obj_t *ebook_list = NULL;
static lv_obj_t *ebook_body_lab = NULL;
static lv_obj_t *ebook_page_lab = NULL;
static int ebook_curr_page = 0;
static int ebook_story_idx = 0;

static const ebook_story_t *ebook_active_story(void)
{
    if (ebook_story_idx < 0 || ebook_story_idx >= ebook_catalog_count) {
        return NULL;
    }
    return &ebook_catalog[ebook_story_idx];
}

static void ebook_show_page(int page)
{
    const ebook_story_t *st = ebook_active_story();
    if (st == NULL || st->page_count <= 0 || st->pages == NULL) {
        return;
    }
    if (page < 0) {
        page = 0;
    }
    if (page >= st->page_count) {
        page = st->page_count - 1;
    }
    ebook_curr_page = page;
    if (ebook_body_lab) {
        lv_label_set_text(ebook_body_lab, st->pages[ebook_curr_page]);
    }
    if (ebook_page_lab) {
        lv_label_set_text_fmt(ebook_page_lab, "%d / %d", ebook_curr_page + 1, st->page_count);
    }
}

static void ebook_page_switch_cb(lv_event_t *e)
{
    if (e->code != LV_EVENT_CLICKED) {
        return;
    }
    const ebook_story_t *st = ebook_active_story();
    if (st == NULL) {
        return;
    }
    const char opt = (char)(intptr_t)e->user_data;
    if (opt == 'n') {
        if (ebook_curr_page + 1 < st->page_count) {
            ebook_show_page(ebook_curr_page + 1);
            ui_disp_full_refr();
        }
    } else if (opt == 'p') {
        if (ebook_curr_page > 0) {
            ebook_show_page(ebook_curr_page - 1);
            ui_disp_full_refr();
        }
    }
}

static void scr11_btn_event_cb(lv_event_t *e)
{
    if (e->code == LV_EVENT_CLICKED) {
        scr_mgr_pop(false);
    }
}

static void scr11_1_btn_event_cb(lv_event_t *e)
{
    if (e->code == LV_EVENT_CLICKED) {
        scr_mgr_pop(false); /* back to TOC */
    }
}

static void ebook_toc_item_cb(lv_event_t *e)
{
    if (e->code != LV_EVENT_CLICKED) {
        return;
    }
    const int idx = (int)(intptr_t)e->user_data;
    if (idx < 0 || idx >= ebook_catalog_count) {
        return;
    }
    ebook_story_idx = idx;
    ebook_curr_page = 0;
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    scr_push_subscreen(SCREEN11_1_ID);
#else
    scr_mgr_push(SCREEN11_1_ID, false);
#endif
}

static void create11(lv_obj_t *parent)
{
    ebook_list = lv_list_create(parent);
    lv_obj_set_size(ebook_list, lv_pct(93), lv_pct(86));
    lv_obj_align(ebook_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_top(ebook_list, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(ebook_list, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ebook_list, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ebook_list, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ebook_list, 0, LV_PART_MAIN);

    for (int i = 0; i < ebook_catalog_count; i++) {
        lv_obj_t *obj = lv_obj_class_create_obj(&lv_list_btn_class, ebook_list);
        lv_obj_class_init_obj(obj);
        lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
        /* 五项目录压到可见区内：约 36px 行高 + 小间距 */
        lv_obj_set_height(obj, 36);
        lv_obj_set_style_text_font(obj, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_ext_click_area(obj, 6);

        lv_obj_t *label = lv_label_create(obj);
        char line[48];
        snprintf(line, sizeof(line), "- %s", ebook_catalog[i].title);
        lv_label_set_text(label, line);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(label, lv_pct(92));
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_add_event_cb(obj, ebook_toc_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    scr_back_btn_create(parent, "eBook", scr11_btn_event_cb);
}

static void entry11(void)
{
    ui_disp_full_refr();
}

static void exit11(void)
{
    ebook_list = NULL;
}

static void destroy11(void)
{
    ebook_list = NULL;
}

static scr_lifecycle_t screen11 = {
    .create = create11,
    .entry = entry11,
    .exit  = exit11,
    .destroy = destroy11,
};

static void create11_1(lv_obj_t *parent)
{
    ebook_curr_page = 0;

    lv_obj_t *cont = lv_obj_create(parent);
    /* 正文高度留出底栏按钮，避免最下两行被遮挡 */
    lv_obj_set_size(cont, lv_pct(100), 210);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(cont, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_right(cont, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(cont, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, LV_PART_MAIN);

    ebook_body_lab = lv_label_create(cont);
    lv_obj_set_width(ebook_body_lab, lv_pct(100));
    lv_obj_set_style_text_font(ebook_body_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_label_set_long_mode(ebook_body_lab, LV_LABEL_LONG_WRAP);

    lv_obj_t *btn_prev = lv_btn_create(parent);
    lv_obj_set_size(btn_prev, 88, 40);
    lv_obj_align(btn_prev, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_prev, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_prev, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_prev, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_prev, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_ext_click_area(btn_prev, 28);
    lv_obj_t *lab_prev = lv_label_create(btn_prev);
    lv_label_set_text(lab_prev, "< Prev");
    lv_obj_set_style_text_color(lab_prev, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lab_prev, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_obj_center(lab_prev);
    lv_obj_add_event_cb(btn_prev, ebook_page_switch_cb, LV_EVENT_CLICKED, (void *)(intptr_t)'p');

    lv_obj_t *btn_next = lv_btn_create(parent);
    lv_obj_set_size(btn_next, 88, 40);
    lv_obj_align(btn_next, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn_next, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_next, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn_next, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn_next, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_ext_click_area(btn_next, 28);
    lv_obj_t *lab_next = lv_label_create(btn_next);
    lv_label_set_text(lab_next, "Next >");
    lv_obj_set_style_text_color(lab_next, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lab_next, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_obj_center(lab_next);
    lv_obj_add_event_cb(btn_next, ebook_page_switch_cb, LV_EVENT_CLICKED, (void *)(intptr_t)'n');

    ebook_page_lab = lv_label_create(parent);
    lv_obj_set_style_text_font(ebook_page_lab, FONT_BOLD_MONO_SIZE_14, LV_PART_MAIN);
    lv_obj_align(ebook_page_lab, LV_ALIGN_BOTTOM_MID, 0, -18);

    const ebook_story_t *st = ebook_active_story();
    const char *back_title = (st && st->title) ? st->title : "eBook";
    /* 顶栏标题过长时截断显示 */
    char short_title[28];
    snprintf(short_title, sizeof(short_title), "%.24s", back_title);
    ebook_show_page(0);
    scr_back_btn_create(parent, short_title, scr11_1_btn_event_cb);
}

static void entry11_1(void)
{
    ui_disp_full_refr();
}

static void exit11_1(void)
{
    ebook_body_lab = NULL;
    ebook_page_lab = NULL;
}

static void destroy11_1(void)
{
    ebook_body_lab = NULL;
    ebook_page_lab = NULL;
}

static scr_lifecycle_t screen11_1 = {
    .create = create11_1,
    .entry = entry11_1,
    .exit = exit11_1,
    .destroy = destroy11_1,
};
#endif
//************************************[ UI ENTRY ]******************************************
static lv_obj_t *menu_keypad;
static lv_timer_t *menu_timer = NULL;

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"

/**
 * 触摸 / 滑动手势状态机（Mesh 页专用）。
 *
 * 设计目标（按用户最新需求）：
 *   1. **左上角返回箭头**点击 → 退回主菜单（不再用右滑退出）。
 *   2. **左右滑** → 切换底部导航帧（LEFT=上一帧 / RIGHT=下一帧）。
 *   3. **无弹窗短按** → USER_PRESS 下一帧；**长按** → SELECT 开菜单。禁用左右滑。
 *   4. **弹窗内**：短按切下一项；长按确认；**禁用上下滑**（问题多）。
 *   5. **TAP 不能注入 SELECT**，否则每次点屏都进 overlay 触发 EPD 全刷。
 */

/** 同向水平位移在时间窗内累加，跨多段 DOWN/UP 拼接（CST3530 易分段上报）。 */
struct w20_swipe_accum_state {
    uint32_t win_t0;
    int16_t last_x;
    int16_t last_y;
    int16_t session_first_x;
    int16_t session_first_y;
    int16_t session_peak_x;
    int16_t session_peak_y;
    bool last_valid;
    bool session_first_valid;
    int32_t accum_left;  // 手指向左滑（dx = last_x - x > 0）
    int32_t accum_right; // 手指向右滑（dx < 0）
    int32_t accum_up;    // 手指向上滑（dy = last_y - y > 0，屏坐标 y 朝下）
    int32_t accum_down;  // 手指向下滑（dy < 0）
    bool touching;
};

/** Mesh 页长滑/分段触摸：比主菜单更宽的时间窗。 */
#ifndef MESH_GESTURE_ACCUM_WINDOW_MS
#define MESH_GESTURE_ACCUM_WINDOW_MS 800
#endif

/** 左右滑切换底部帧的阈值（跟手过程中触发）。 */
#ifndef MESH_PAGE_SLIDE_DISTANCE
#define MESH_PAGE_SLIDE_DISTANCE 18
#endif

/** 松手时稍低阈值：避免「差一点变 TAP 又切下一帧」。 */
#ifndef MESH_PAGE_SLIDE_RELEASE_DISTANCE
#define MESH_PAGE_SLIDE_RELEASE_DISTANCE 12
#endif

/**
 * 水平位移达到此值则不再判为 TAP（避免「想左右滑切帧」被当成短按）。
 * 应明显小于 MESH_PAGE_SLIDE_DISTANCE。
 */
#ifndef MESH_HORIZONTAL_CANCEL_TAP_PX
#define MESH_HORIZONTAL_CANCEL_TAP_PX 8
#endif

/** 左上角返回箭头触摸热区（与 drawCommonHeader 电池左侧箭头对齐）。 */
#ifndef MESH_BACK_BTN_W
#define MESH_BACK_BTN_W 28
#endif
#ifndef MESH_BACK_BTN_H
#define MESH_BACK_BTN_H 22
#endif

/** 无弹窗时长按开菜单阈值。过短易把短按当成开窗。 */
#ifndef MESH_TOUCH_LONG_MS
#define MESH_TOUCH_LONG_MS 600
#endif

/** TAP 位移上限：水平/垂直任一方向绝对位移超过此值则不再判为 TAP。 */
#ifndef MESH_TAP_SUPPRESS_PX
#define MESH_TAP_SUPPRESS_PX 8
#endif

/** 按下/触摸状态：跨多段 DOWN/UP 期间累积长按时间、识别 TAP/LONG/手势。 */
struct w20_mesh_touch_press_state {
    bool active;
    bool long_fired;
    bool gesture_claimed;
    uint32_t press_start_ms;
    int16_t first_x;
    int16_t first_y;
    int16_t last_x;
    int16_t last_y;
    int16_t session_max_adx;
    int16_t session_max_ady;
};

static struct w20_swipe_accum_state s_swipe;
static struct w20_mesh_touch_press_state s_mesh_press;

/** Release 后屏蔽窗口：忽略此窗口内的下一次 DOWN，避免 CST3530 多段 cycle
 *  被识别为多次 TAP → 视觉上"一次跳两页"。窗口期内 touch 仍能上报 swipe 触发 mesh 退出。 */
static uint32_t s_mesh_tap_release_deadline_ms = 0;

/** LONG(SELECT) 后抑制 TAP：覆盖「松手误切页」与「overlay 全刷慢再点误推光标」。 */
static uint32_t s_mesh_tap_suppress_until_ms = 0;
/** LONG 已消费本次按压：手指抬起前忽略新 press/TAP（overlay 关闭后仍可能同一次按压）。 */
static bool s_mesh_ignore_press_until_up = false;

#ifndef MESH_TAP_RELEASE_GUARD_MS
#define MESH_TAP_RELEASE_GUARD_MS 250
#endif

/** 长按后抑制短按的时长（仅「开菜单同一按下」的松手误触；勿拉太长否则弹窗后要点两次才关）。 */
#ifndef MESH_LONG_TAP_SUPPRESS_MS
#define MESH_LONG_TAP_SUPPRESS_MS 400
#endif

/** overlay 内长按确认阈值：略长于开菜单，避免刚开窗就误确认。 */
#ifndef MESH_OVERLAY_CONFIRM_LONG_MS
#define MESH_OVERLAY_CONFIRM_LONG_MS 700
#endif

static void mesh_arm_tap_suppress(uint32_t extra_ms)
{
    const uint32_t until = millis() + extra_ms;
    if (until > s_mesh_tap_suppress_until_ms) {
        s_mesh_tap_suppress_until_ms = until;
    }
    s_mesh_ignore_press_until_up = true;
    if (until > s_mesh_tap_release_deadline_ms) {
        s_mesh_tap_release_deadline_ms = until;
    }
}

static bool mesh_tap_suppressed(uint32_t now)
{
    return now < s_mesh_tap_suppress_until_ms || now < s_mesh_tap_release_deadline_ms;
}

static void mesh_touch_press_reset(void)
{
    memset(&s_mesh_press, 0, sizeof(s_mesh_press));
}

static void swipe_accum_reset(void)
{
    memset(&s_swipe, 0, sizeof(s_swipe));
    mesh_touch_press_reset();
    s_mesh_tap_release_deadline_ms = millis() + MESH_TAP_RELEASE_GUARD_MS;
}

/** 长按已触发时清滑动累计，但保留 touching/press，避免误认新按下。 */
static void swipe_accum_clear_motion(void)
{
    s_swipe.win_t0 = 0;
    s_swipe.last_valid = false;
    s_swipe.accum_left = 0;
    s_swipe.accum_right = 0;
    s_swipe.accum_up = 0;
    s_swipe.accum_down = 0;
}

static void swipe_accum_expire(uint32_t now, uint32_t window_ms)
{
    if (s_swipe.win_t0 != 0 && !s_swipe.touching && (now - s_swipe.win_t0) > window_ms)
        swipe_accum_reset();
}

static int16_t mesh_touch_abs16(int16_t v)
{
    return v < 0 ? (int16_t)-v : v;
}

/** 主菜单只用 X 累积（Y 方向无 touch 坐标源），保留独立函数不污染 mesh 的 Y 轴。 */
static void swipe_accum_add_x(int16_t x, uint32_t now, uint32_t window_ms)
{
    if (s_swipe.win_t0 == 0 || (now - s_swipe.win_t0) > window_ms) {
        s_swipe.win_t0 = now;
        s_swipe.accum_left = 0;
        s_swipe.accum_right = 0;
        s_swipe.last_valid = false;
        s_swipe.session_first_x = x;
        s_swipe.session_first_valid = true;
        s_swipe.session_peak_x = x;
    }

    if (x > s_swipe.session_peak_x) {
        s_swipe.session_peak_x = x;
    }

    if (s_swipe.last_valid) {
        int16_t dx = s_swipe.last_x - x;
        if (dx > 0) {
            s_swipe.accum_left += dx;
        } else if (dx < 0) {
            s_swipe.accum_right += -dx;
        }
    }
    s_swipe.last_x = x;
    s_swipe.last_valid = true;
}

static void swipe_accum_add_xy(int16_t x, int16_t y, uint32_t now, uint32_t window_ms)
{
    if (s_swipe.win_t0 == 0 || (now - s_swipe.win_t0) > window_ms) {
        s_swipe.win_t0 = now;
        s_swipe.accum_left = 0;
        s_swipe.accum_right = 0;
        s_swipe.accum_up = 0;
        s_swipe.accum_down = 0;
        s_swipe.last_valid = false;
        s_swipe.session_first_x = x;
        s_swipe.session_first_y = y;
        s_swipe.session_first_valid = true;
        s_swipe.session_peak_x = x;
        s_swipe.session_peak_y = y;
    }

    if (x > s_swipe.session_peak_x) {
        s_swipe.session_peak_x = x;
    }
    if (y > s_swipe.session_peak_y) {
        s_swipe.session_peak_y = y;
    }

    if (s_mesh_press.active) {
        const int16_t adx = mesh_touch_abs16((int16_t)(x - s_mesh_press.first_x));
        const int16_t ady = mesh_touch_abs16((int16_t)(y - s_mesh_press.first_y));
        if (adx > s_mesh_press.session_max_adx) {
            s_mesh_press.session_max_adx = adx;
        }
        if (ady > s_mesh_press.session_max_ady) {
            s_mesh_press.session_max_ady = ady;
        }
    }

    if (s_swipe.last_valid) {
        int16_t dx = s_swipe.last_x - x;
        if (dx > 0) {
            s_swipe.accum_left += dx;
        } else if (dx < 0) {
            s_swipe.accum_right += -dx;
        }
        int16_t dy = s_swipe.last_y - y;
        if (dy > 0) {
            s_swipe.accum_up += dy;
        } else if (dy < 0) {
            s_swipe.accum_down += -dy;
        }
    }
    s_swipe.last_x = x;
    s_swipe.last_y = y;
    s_swipe.last_valid = true;
}

static bool swipe_accum_try_fire(const char *tag)
{
    if (!ui_get_gesture_dir)
        return false;

    if (s_swipe.accum_left >= UI_SLIDING_DISTANCE && s_swipe.accum_left > s_swipe.accum_right) {
        W20_UI_LOG("gesture_%s LEFT accum_L=%ld accum_R=%ld thr=%d win=%ums", tag, (long)s_swipe.accum_left,
                   (long)s_swipe.accum_right, UI_SLIDING_DISTANCE, (unsigned)(millis() - s_swipe.win_t0));
        ui_get_gesture_dir(LV_DIR_LEFT);
        swipe_accum_reset();
        return true;
    }
    if (s_swipe.accum_right >= UI_SLIDING_DISTANCE && s_swipe.accum_right > s_swipe.accum_left) {
        W20_UI_LOG("gesture_%s RIGHT accum_L=%ld accum_R=%ld thr=%d win=%ums", tag, (long)s_swipe.accum_left,
                   (long)s_swipe.accum_right, UI_SLIDING_DISTANCE, (unsigned)(millis() - s_swipe.win_t0));
        ui_get_gesture_dir(LV_DIR_RIGHT);
        swipe_accum_reset();
        return true;
    }
    return false;
}

static void mesh_inject_input(input_broker_event ev, int16_t x, int16_t y)
{
#if defined(MESHCORE_DECK_SHELL)
    /* CST3530 → MeshCore UITask：短按下一页，长按确认；不用空 InputBroker。 */
    (void)x;
    (void)y;
    if (ev == INPUT_BROKER_USER_PRESS) {
        w20_meshcore_ui_nav_next();
    } else if (ev == INPUT_BROKER_SELECT) {
        w20_meshcore_ui_nav_select();
    }
#else
    if (!inputBroker) {
        return;
    }
    InputEvent e = {};
    e.source = "w20_mesh_touch";
    e.inputEvent = ev;
    e.touchX = (uint16_t)(x < 0 ? 0 : x);
    e.touchY = (uint16_t)(y < 0 ? 0 : y);
    inputBroker->injectInputEvent(&e);
#endif
}

/** TAP 判定：session 期间峰值位移 |peak_x - first_x| 极小，且无 Y 方向位移超过抑制阈值。 */
static bool mesh_gesture_session_is_tap(void)
{
    if (!s_swipe.session_first_valid) {
        return false;
    }
    const int32_t dx = (int32_t)s_swipe.session_peak_x - (int32_t)s_swipe.session_first_x;
    if (dx < -MESH_TAP_SUPPRESS_PX || dx > MESH_TAP_SUPPRESS_PX) {
        return false;
    }
    if (s_swipe.session_peak_y != 0) {
        const int32_t dy = (int32_t)s_swipe.session_peak_y - (int32_t)s_swipe.session_first_y;
        if (dy < -MESH_TAP_SUPPRESS_PX || dy > MESH_TAP_SUPPRESS_PX) {
            return false;
        }
    }
    return true;
}

/** 是否已经产生了明显水平滑动——用于决定释放时是 TAP 还是 swipe。 */
static bool mesh_slide_motion_detected(void)
{
    const int32_t thr = MESH_PAGE_SLIDE_RELEASE_DISTANCE;
    return s_swipe.accum_left >= thr || s_swipe.accum_right >= thr;
}

static bool mesh_horizontal_swipe_intent(void)
{
    return s_swipe.accum_right >= MESH_HORIZONTAL_CANCEL_TAP_PX || s_swipe.accum_left >= MESH_HORIZONTAL_CANCEL_TAP_PX;
}

static bool mesh_back_btn_hit(int16_t x, int16_t y)
{
    return x >= 0 && x < MESH_BACK_BTN_W && y >= 0 && y < MESH_BACK_BTN_H;
}

/**
 * Mesh 页手势：
 *   - 无弹窗：短按切下一帧；长按开菜单；左上角点击回主菜单。禁用左右滑。
 *   - 有弹窗：禁用滑动切项；短按切下一项；长按确认；左上角点击仍回主菜单。
 */
static bool mesh_swipe_try_fire(int16_t x, int16_t y, bool on_release)
{
    if (!w20_shell_mesh_screen_allowed()) {
        return false;
    }

    /* 弹窗内不处理滑动（切项改纯短按）。 */
    if (graphics::NotificationRenderer::isOverlayBannerShowing()) {
        return false;
    }

    if (s_mesh_press.gesture_claimed) {
        return false;
    }

    /* 左右滑已禁用：有水平位移也不切帧，避免与短按打架。 */
    if (mesh_horizontal_swipe_intent() || mesh_slide_motion_detected()) {
        if (on_release) {
            W20_UI_LOG("gesture_mesh swipe ignored (L=%ld R=%ld)", (long)s_swipe.accum_left,
                       (long)s_swipe.accum_right);
            swipe_accum_clear_motion();
            return true;
        }
        return false;
    }

    /* 仅 TAP：左上角返回；否则短按切下一帧。 */
    if (on_release && mesh_gesture_session_is_tap()) {
        const uint32_t now = millis();
        if (s_mesh_press.long_fired || s_mesh_press.gesture_claimed || mesh_tap_suppressed(now) ||
            s_mesh_ignore_press_until_up) {
            W20_UI_LOG("gesture_mesh TAP ignored (long/suppress) (%d,%d)", (int)x, (int)y);
            swipe_accum_clear_motion();
            return true;
        }
        if (mesh_back_btn_hit(x, y)) {
            W20_UI_LOG("gesture_mesh BACK btn tap (%d,%d) -> deck menu", (int)x, (int)y);
            ui_deckpro_exit_mesh();
            swipe_accum_reset();
            return true;
        }
        W20_UI_LOG("gesture_mesh TAP (%d,%d) -> USER_PRESS (next frame)", (int)x, (int)y);
        mesh_inject_input(INPUT_BROKER_USER_PRESS, x, y);
        swipe_accum_clear_motion();
        return true;
    }

    return false;
}

/** 仅当整数 SOC 变化时更新电量 Label，避免每秒 set_text 触发 full_refresh EPD。 */
static bool menu_taskbar_sync_battery_percent_labels(int percent)
{
    if (menu_taskbar_battery == nullptr || menu_taskbar_battery_percent == nullptr) {
        return false;
    }
    if (taskbar_statue[TASKBAR_ID_BATTERY_PERCENT] == (uint16_t)percent) {
        return false;
    }
    lv_label_set_text_fmt(menu_taskbar_battery_percent, "%d", percent);
    lv_label_set_text_fmt(menu_taskbar_battery, "%s", ui_battery_lv_symbol());
    taskbar_statue[TASKBAR_ID_BATTERY_PERCENT] = (uint16_t)percent;
    return true;
}

/** VBUS 连续 2 次一致才切换，减轻 inputPresent 抖动导致反复刷墨。 */
static bool menu_taskbar_vbus_in_stable(void)
{
    static int8_t s_last_sample = -1;
    static uint8_t s_same_count = 0;
    static int8_t s_stable = -1;

    const int8_t sample = ui_battery_vbus_in() ? 1 : 0;
    if (sample == s_last_sample) {
        if (s_same_count < 255) {
            s_same_count++;
        }
    } else {
        s_last_sample = sample;
        s_same_count = 1;
    }

    if (s_same_count >= 2) {
        s_stable = sample;
    }

    if (s_stable < 0) {
        return ui_battery_vbus_in();
    }
    return s_stable != 0;
}

void ui_deckpro_refresh_taskbar_battery(void)
{
    (void)menu_taskbar_sync_battery_percent_labels((int)ui_battery_percent());
}

void ui_deckpro_refresh_taskbar_time(void)
{
    if (menu_taskbar_time == nullptr) {
        return;
    }
    static int s_last_packed = -1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    int packed;
    if (ui_clock_get_local_hm(&hour, &minute)) {
        packed = (int)hour * 60 + (int)minute;
    } else {
        packed = -1;
    }
    if (packed == s_last_packed) {
        return;
    }
    s_last_packed = packed;
    if (packed >= 0) {
        lv_label_set_text_fmt(menu_taskbar_time, "%02d:%02d", hour, minute);
    } else {
        lv_label_set_text(menu_taskbar_time, "12:00");
    }
}

void ui_deckpro_mesh_swipe_back(void)
{
    ui_deckpro_exit_mesh();
}

/** Meshtastic 页 overlay：短按切下一项；长按确认；禁用滑动切项；左上角返回主菜单。 */
static void poll_mesh_overlay_touch_only(int16_t x, int16_t y, bool pressed, uint32_t now)
{
    if (pressed) {
        if (s_mesh_ignore_press_until_up) {
            return;
        }
        if (!s_mesh_press.active) {
            /* 滑动/短按松手后的 guard：避免 CST3530 多段 cycle 立刻新开会话再 TAP */
            if (mesh_tap_suppressed(now)) {
                return;
            }
            s_mesh_press.active = true;
            s_mesh_press.long_fired = false;
            s_mesh_press.gesture_claimed = false;
            s_mesh_press.press_start_ms = now;
            s_mesh_press.first_x = x;
            s_mesh_press.first_y = y;
            s_mesh_press.last_x = x;
            s_mesh_press.last_y = y;
            s_mesh_press.session_max_adx = 0;
            s_mesh_press.session_max_ady = 0;
        } else {
            s_mesh_press.last_x = x;
            s_mesh_press.last_y = y;
            const int16_t adx = (int16_t)abs((int)x - (int)s_mesh_press.first_x);
            const int16_t ady = (int16_t)abs((int)y - (int)s_mesh_press.first_y);
            if (adx > s_mesh_press.session_max_adx)
                s_mesh_press.session_max_adx = adx;
            if (ady > s_mesh_press.session_max_ady)
                s_mesh_press.session_max_ady = ady;
        }

        if (s_mesh_press.gesture_claimed || s_mesh_press.long_fired) {
            return;
        }

        if (s_mesh_press.session_max_adx <= MESH_TAP_SUPPRESS_PX &&
            s_mesh_press.session_max_ady <= MESH_TAP_SUPPRESS_PX &&
            (now - s_mesh_press.press_start_ms) >= MESH_OVERLAY_CONFIRM_LONG_MS) {
            /* 左上角热区不触发确认，留给松手返回 */
            if (mesh_back_btn_hit(s_mesh_press.first_x, s_mesh_press.first_y)) {
                return;
            }
            s_mesh_press.long_fired = true;
            s_mesh_press.gesture_claimed = true;
            s_mesh_ignore_press_until_up = true;
            W20_UI_LOG("mesh_touch LONG (%d,%d) overlay CONFIRM", (int)x, (int)y);
            mesh_inject_input(INPUT_BROKER_SELECT, x, y);
            mesh_arm_tap_suppress(MESH_LONG_TAP_SUPPRESS_MS);
        }
    } else if (s_mesh_press.active) {
        const int16_t tap_x = s_mesh_press.last_x;
        const int16_t tap_y = s_mesh_press.last_y;
        const uint32_t dur = now - s_mesh_press.press_start_ms;
        if (s_mesh_press.long_fired || s_mesh_ignore_press_until_up) {
            W20_UI_LOG("mesh_touch TAP ignored after LONG overlay (%d,%d)", (int)tap_x, (int)tap_y);
        } else if (!s_mesh_press.gesture_claimed && dur > 0 && dur < MESH_OVERLAY_CONFIRM_LONG_MS &&
                   s_mesh_press.session_max_adx <= MESH_TAP_SUPPRESS_PX &&
                   s_mesh_press.session_max_ady <= MESH_TAP_SUPPRESS_PX) {
            if (mesh_back_btn_hit(tap_x, tap_y) || mesh_back_btn_hit(s_mesh_press.first_x, s_mesh_press.first_y)) {
                W20_UI_LOG("mesh_touch BACK btn (overlay) (%d,%d) -> deck menu", (int)tap_x, (int)tap_y);
                ui_deckpro_exit_mesh();
            } else {
                /* 短按：切下一项（不关窗） */
                W20_UI_LOG("mesh_touch TAP (%d,%d) overlay NEXT item", (int)tap_x, (int)tap_y);
                mesh_inject_input(INPUT_BROKER_USER_PRESS, tap_x, tap_y);
            }
            mesh_arm_tap_suppress(MESH_TAP_RELEASE_GUARD_MS);
        }
        mesh_touch_press_reset();
        s_mesh_ignore_press_until_up = false;
        swipe_accum_clear_motion();
    } else if (!pressed) {
        s_mesh_ignore_press_until_up = false;
    }
}

/** Meshtastic 页不跑 lv_task_handler，须直接读 CST3530 做触摸/滑动手势。 */
static void poll_gesture_from_touch_chip(void)
{
    int16_t x = 0;
    int16_t y = 0;
    bool pressed = touch.getPoint(&x, &y, 1) > 0;
    const uint32_t now = millis();

    /* overlay：短按切项；长按确认；左上角返回主菜单。禁用滑动切项。 */
    if (graphics::NotificationRenderer::isOverlayBannerShowing()) {
        if (pressed) {
            s_swipe.touching = true;
            if (!s_mesh_ignore_press_until_up) {
                swipe_accum_add_xy(x, y, now, MESH_GESTURE_ACCUM_WINDOW_MS);
            }
        } else {
            s_swipe.touching = false;
        }
        poll_mesh_overlay_touch_only(x, y, pressed, now);
        return;
    }

    swipe_accum_expire(now, MESH_GESTURE_ACCUM_WINDOW_MS);

    if (pressed) {
        /* LONG 已消费本次按压：手指抬起前不开启新 TAP/切页会话（overlay 可能已关）。 */
        if (s_mesh_ignore_press_until_up) {
            s_swipe.touching = true;
            return;
        }

        /* Release 后屏蔽窗口内：忽略下一次 DOWN（避免 CST3530 多段 cycle 被识别为
         * 连续 TAP）。swipe 仍可继续累积（用户在窗口期内继续滑仍能退 mesh）。 */
        const bool in_release_guard = mesh_tap_suppressed(now);

        /* 新按下：初始化 press 状态机（屏蔽窗口内：不重启 press，避免"一次跳两页"） */
        if (!s_swipe.touching && !in_release_guard) {
            s_mesh_press.active = true;
            s_mesh_press.long_fired = false;
            s_mesh_press.gesture_claimed = false;
            s_mesh_press.press_start_ms = now;
            s_mesh_press.first_x = x;
            s_mesh_press.first_y = y;
            s_mesh_press.last_x = x;
            s_mesh_press.last_y = y;
            s_mesh_press.session_max_adx = 0;
            s_mesh_press.session_max_ady = 0;
        } else if (!in_release_guard) {
            s_mesh_press.last_x = x;
            s_mesh_press.last_y = y;
        }

        s_swipe.touching = true;
        swipe_accum_add_xy(x, y, now, MESH_GESTURE_ACCUM_WINDOW_MS);

        const bool slide_motion = mesh_slide_motion_detected();
        if (mesh_swipe_try_fire(x, y, false)) {
            s_mesh_press.gesture_claimed = true;
            swipe_accum_clear_motion();
        } else if (!s_mesh_press.gesture_claimed && !s_mesh_press.long_fired) {
            if (slide_motion) {
                /* 已经产生明显水平位移：等 release 后由 try_fire 切帧 */
            } else if ((now - s_mesh_press.press_start_ms) >= MESH_TOUCH_LONG_MS) {
                /* 左上角热区：长按不弹菜单，松手走返回 */
                if (mesh_back_btn_hit(s_mesh_press.first_x, s_mesh_press.first_y)) {
                    /* wait for release */
                } else {
                    s_mesh_press.long_fired = true;
                    s_mesh_press.gesture_claimed = true;
                    s_mesh_ignore_press_until_up = true;
                    W20_UI_LOG("mesh_touch LONG (%d,%d)", (int)x, (int)y);
                    mesh_inject_input(INPUT_BROKER_SELECT, x, y);
                    mesh_arm_tap_suppress(MESH_LONG_TAP_SUPPRESS_MS);
                    swipe_accum_clear_motion();
                }
            }
        }
    } else {
        if (s_swipe.touching) {
            const int16_t rel_x = s_mesh_press.last_x;
            const int16_t rel_y = s_mesh_press.last_y;
            const bool long_or_suppress =
                s_mesh_press.long_fired || s_mesh_ignore_press_until_up || mesh_tap_suppressed(now);
            if (!long_or_suppress && mesh_swipe_try_fire(rel_x, rel_y, true)) {
                s_mesh_press.gesture_claimed = true;
            } else if (long_or_suppress) {
                W20_UI_LOG("gesture_mesh release ignored (long/suppress) (%d,%d)", (int)rel_x, (int)rel_y);
                swipe_accum_reset();
            } else if (s_mesh_press.active && !s_mesh_press.gesture_claimed && !s_mesh_press.long_fired &&
                       !mesh_slide_motion_detected() && !mesh_horizontal_swipe_intent()) {
                /* try_fire 未吃掉的短按兜底（含左上角返回） */
                const uint32_t dur = now - s_mesh_press.press_start_ms;
                if (dur > 0 && dur < MESH_TOUCH_LONG_MS && !mesh_tap_suppressed(now)) {
                    if (mesh_back_btn_hit(rel_x, rel_y) ||
                        mesh_back_btn_hit(s_mesh_press.first_x, s_mesh_press.first_y)) {
                        W20_UI_LOG("gesture_mesh BACK btn tap fallback (%d,%d) -> deck menu", (int)rel_x,
                                   (int)rel_y);
                        ui_deckpro_exit_mesh();
                    } else {
                        W20_UI_LOG("mesh_touch TAP (%d,%d) -> USER_PRESS", (int)rel_x, (int)rel_y);
                        mesh_inject_input(INPUT_BROKER_USER_PRESS, rel_x, rel_y);
                    }
                    mesh_arm_tap_suppress(MESH_TAP_RELEASE_GUARD_MS);
                }
            } else if (mesh_horizontal_swipe_intent() && !mesh_slide_motion_detected()) {
                W20_UI_LOG("mesh_touch TAP ignored (hswipe L=%ld R=%ld)", (long)s_swipe.accum_left,
                           (long)s_swipe.accum_right);
                swipe_accum_reset();
            }
        }
        s_swipe.touching = false;
        s_mesh_ignore_press_until_up = false;
        mesh_touch_press_reset();
        swipe_accum_expire(now, MESH_GESTURE_ACCUM_WINDOW_MS);
    }
}

void ui_deckpro_poll_gesture(void)
{
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    if (w20_shell_mesh_screen_allowed()) {
        poll_gesture_from_touch_chip();
        return;
    }
#endif

    if (!ui_get_gesture_dir)
        return;

    lv_indev_data_t data;
    lv_indev_t *indev_pointer = lv_indev_get_next(NULL);
    if (!indev_pointer)
        return;

    _lv_indev_read(indev_pointer, &data);
    const uint32_t now = millis();

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    swipe_accum_expire(now, UI_GESTURE_ACCUM_WINDOW_MS);

    if (data.state == LV_INDEV_STATE_PR) {
        s_swipe.touching = true;
        /* 主菜单只关心 X 方向 swipe：单独 add_x，不污染 Y 累积。 */
        swipe_accum_add_x((int16_t)data.point.x, now, UI_GESTURE_ACCUM_WINDOW_MS);
        swipe_accum_try_fire("menu");
    } else {
        if (s_swipe.touching) {
            swipe_accum_try_fire("menu");
        }
        s_swipe.touching = false;
        swipe_accum_expire(now, UI_GESTURE_ACCUM_WINDOW_MS);
    }
#else
    static lv_point_t start_point;
    static lv_point_t track_point;
    static bool is_press = false;
    static bool gesture_reported = false;

    if (data.state == LV_INDEV_STATE_PR) {
        if (!is_press) {
            is_press = true;
            start_point = track_point = data.point;
            gesture_reported = false;
        } else {
            track_point = data.point;
        }
        if (gesture_reported)
            return;
        lv_coord_t diff_x = start_point.x - track_point.x;
        if (diff_x > UI_SLIDING_DISTANCE) {
            ui_get_gesture_dir(LV_DIR_LEFT);
            gesture_reported = true;
        } else if (diff_x < -UI_SLIDING_DISTANCE) {
            ui_get_gesture_dir(LV_DIR_RIGHT);
            gesture_reported = true;
        }
    } else {
        if (is_press && !gesture_reported) {
            lv_coord_t diff_x = start_point.x - track_point.x;
            if (diff_x > UI_SLIDING_DISTANCE)
                ui_get_gesture_dir(LV_DIR_LEFT);
            else if (diff_x < -UI_SLIDING_DISTANCE)
                ui_get_gesture_dir(LV_DIR_RIGHT);
        }
        is_press = false;
        gesture_reported = false;
    }
#endif
}

static void indev_get_gesture_dir(lv_timer_t *t)
{
    (void)t;
    ui_deckpro_poll_gesture();
}

static void menu_keypay_get_event(lv_timer_t *t)
{
    static int sec = 0;
    static int press = false;
    char keypay_v;
    int ret = ui_input_get_keypay_val(&keypay_v);

    if(ret > 0)
    {
        sec = 0;
        press = true;
        ui_input_set_keypay_flag();
        lv_label_set_text_fmt(menu_keypad, "%c", keypay_v);
    }

    if(press){
        sec++;
        if(sec > 20) {
            sec = 0;
            press = false;
            lv_label_set_text(menu_keypad, " ");
        }
    }
}

static void menu_taskbar_epd_sync(void)
{
    if (menu_taskbar == nullptr) {
        return;
    }
    lv_obj_invalidate(menu_taskbar);
    ui_disp_full_refr();
}

/** 时钟+电量变更后异步 FULL 刷 EPD。
 * 勿在 RTC 首次有效时调 present_menu：会与 Deck 子菜单 pop/prepare 抢 gen，
 * 卡满 8s 超时后出现「LVGL 已回主菜单、墨水仍停在子页」。
 * present_menu 仅保留 Mesh→九宫格 / 开机路径。 */
static void menu_taskbar_status_epd_sync(void)
{
    if (menu_taskbar_time == nullptr) {
        return;
    }
    lv_obj_invalidate(menu_taskbar_time);
    if (menu_taskbar != nullptr) {
        lv_obj_invalidate(menu_taskbar);
    }
    menu_taskbar_epd_sync();
}

/** 更新 WiFi 图标显隐，返回是否有可见变化。 */
static bool menu_taskbar_apply_wifi_labels(void)
{
    if (menu_taskbar_wifi == nullptr) {
        return false;
    }

    const bool wifi = ui_wifi_is_connected();
    if (taskbar_statue[TASKBAR_ID_WIFI] == (uint16_t)wifi) {
        return false;
    }

    if (wifi) {
        lv_obj_clear_flag(menu_taskbar_wifi, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(menu_taskbar_wifi, LV_OBJ_FLAG_HIDDEN);
    }
    taskbar_statue[TASKBAR_ID_WIFI] = wifi;
    return true;
}

/** 更新电量/充电 LVGL。
 * @param epd_track_percent true 时整数 % 变化也更新 Label 并计入 changed（60s 路径）。 */
static bool menu_taskbar_apply_battery_labels(bool epd_track_percent)
{
    bool changed = false;
    const bool finish = ui_battery_charge_complete();
    const int percent = (int)ui_battery_percent();

    if (taskbar_statue[TASKBAR_ID_CHARGE_FINISH] != finish) {
        if (finish) {
            lv_label_set_text_fmt(menu_taskbar_charge, "%s", LV_SYMBOL_OK);
        } else {
            lv_label_set_text_fmt(menu_taskbar_charge, "%s", LV_SYMBOL_CHARGE);
        }
        taskbar_statue[TASKBAR_ID_CHARGE_FINISH] = finish;
        changed = true;
    }

    if (epd_track_percent && menu_taskbar_sync_battery_percent_labels(percent)) {
        changed = true;
    }

    const bool charge = menu_taskbar_vbus_in_stable();
    if (taskbar_statue[TASKBAR_ID_CHARGE] != charge) {
        if (charge) {
            lv_obj_clear_flag(menu_taskbar_charge, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(menu_taskbar_charge, LV_OBJ_FLAG_HIDDEN);
        }
        taskbar_statue[TASKBAR_ID_CHARGE] = charge;
        changed = true;
    }
    return changed;
}

/** 主菜单顶栏：时钟 + 电量 + WiFi 同帧更新，一次异步 EPD。 */
static void menu_taskbar_refresh_status_epd(void)
{
    ui_deckpro_refresh_taskbar_time();
    menu_taskbar_apply_battery_labels(true);
    menu_taskbar_apply_wifi_labels();
    menu_taskbar_status_epd_sync();
}

/** 每秒检查：分钟变化或 RTC 首次有效时，时钟+电量同步刷 EPD。 */
static bool menu_taskbar_update_time_if_needed(void)
{
    static int last_packed = -1;
    uint8_t hour = 0;
    uint8_t minute = 0;
    int packed;
    if (ui_clock_get_local_hm(&hour, &minute)) {
        packed = (int)hour * 60 + (int)minute;
    } else {
        packed = -1;
    }
    if (packed == last_packed) {
        return false;
    }
    last_packed = packed;
    menu_taskbar_refresh_status_epd();
    return true;
}

/** 主菜单：仅更新 taskbar 文案（不触发 EPD；FULL 刷由 pop/prepare 或 boot timer 负责）。 */
static void menu_taskbar_refresh_all(void)
{
    ui_deckpro_refresh_taskbar_time();
    menu_taskbar_apply_battery_labels(true);
    menu_taskbar_apply_wifi_labels();
}

static int s_menu_taskbar_timer_sec = 0;

static void menu_taskbar_update_timer_cb(lv_timer_t *t)
{
    (void)t;
    s_menu_taskbar_timer_sec++;

    const bool epd_for_time = menu_taskbar_update_time_if_needed();

    const bool wifi_changed = menu_taskbar_apply_wifi_labels();
    if (wifi_changed) {
        menu_taskbar_epd_sync();
    }

    /* 每秒检测插拔线/充满态（不含整数 %）；插 Type-C 后不必等 60s。 */
    const bool charge_ui_changed = menu_taskbar_apply_battery_labels(false);
    if (charge_ui_changed && !epd_for_time) {
        menu_taskbar_epd_sync();
    }

    /* 60s：仅当 SOC 整数变化时更新 Label 并刷 EPD。 */
    if (s_menu_taskbar_timer_sec % MENU_TASKBAR_REFRESH_SEC == 0) {
        ui_deckpro_refresh_taskbar_time();
        const bool pct_changed = menu_taskbar_apply_battery_labels(true);
        const bool wifi_at_60 = menu_taskbar_apply_wifi_labels();
        if (!epd_for_time && (pct_changed || wifi_at_60)) {
            menu_taskbar_epd_sync();
        }
    }
}

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
extern "C" void ui_deckpro_on_power_status_changed(void)
{
    if (w20_shell_mesh_screen_allowed()) {
        return;
    }

    const int top = scr_mgr_top_id();
    if (top == SCREEN6_ID) {
        if (!scr6_battery_charge_epd_changed()) {
            return;
        }
        scr6_battery_update();
        lv_obj_t *scr = lv_scr_act();
        if (scr != nullptr) {
            lv_obj_invalidate(scr);
        }
        ui_disp_full_refr();
        return;
    }

    if (scr_mgr_is_root_top() && top == SCREEN0_ID) {
        if (menu_taskbar_apply_battery_labels(false)) {
            menu_taskbar_epd_sync();
        }
        return;
    }

    /* Setting 页：充电态变化时同步 RGB Status（插拔 Type-C / IP5356 late-ready）。 */
    if (top == SCREEN2_ID) {
        setting_rgb_sync_charging(true);
    }
}

extern "C" void ui_deckpro_poll_power_ui(void)
{
#if USE_IP5356_CHARGER
    if (w20_deck_sleep_active()) {
        return;
    }

    static uint32_t s_last_ms = 0;
    static int8_t s_last_vbus = -1;
    static int8_t s_last_chg = -1;
    const uint32_t now = millis();
    if (now - s_last_ms < 500) {
        return;
    }
    s_last_ms = now;

    const int8_t vbus = ui_battery_vbus_in() ? 1 : 0;
    const int8_t chg = ui_battery_charging() ? 1 : 0;
    if (s_last_vbus < 0) {
        s_last_vbus = vbus;
        s_last_chg = chg;
        return;
    }

    bool edge = false;
    if (vbus != s_last_vbus) {
        s_last_vbus = vbus;
        edge = true;
        powerRefreshStatus();
    }
    if (chg != s_last_chg) {
        s_last_chg = chg;
        edge = true;
    }
    if (!edge) {
        return;
    }

    /* Setting 页停留期间插拔/起停充：立刻同步 RGB Status 文案并刷墨。 */
    if (scr_mgr_top_id() == SCREEN2_ID) {
        setting_rgb_sync_charging(true);
    }
#endif
}
#endif

extern "C" void ui_deckpro_pause_aux_timers(void)
{
    if(touch_chk_timer) {
        lv_timer_pause(touch_chk_timer);
    }
    if(taskbar_update_timer) {
        lv_timer_pause(taskbar_update_timer);
    }
    if (wifi_ota_timer) {
        lv_timer_pause(wifi_ota_timer);
    }
}

extern "C" void ui_deckpro_resume_aux_timers(void)
{
    s_menu_taskbar_timer_sec = 0;
    if (taskbar_update_timer) {
        lv_timer_reset(taskbar_update_timer);
        lv_timer_resume(taskbar_update_timer);
    }
    if (wifi_ota_timer) {
        lv_timer_resume(wifi_ota_timer);
    }
}

extern "C" void ui_deckpro_menu_prepare_for_epd(void)
{
    if (menu_screen1 != nullptr) {
        lv_obj_scroll_to(menu_screen1, 0, 0, LV_ANIM_OFF);
        lv_obj_update_layout(menu_screen1);
    }
    if (menu_screen2 != nullptr) {
        lv_obj_scroll_to(menu_screen2, 0, 0, LV_ANIM_OFF);
        lv_obj_update_layout(menu_screen2);
    }
}

extern "C" bool ui_deckpro_lvgl_touch_suppressed(void)
{
    if (!ui_get_gesture_dir) {
        return false;
    }
    return s_swipe.touching && (s_swipe.accum_left >= 8 || s_swipe.accum_right >= 8);
}

extern "C" void ui_deckpro_gesture_reset(void)
{
    swipe_accum_reset();
    s_mesh_tap_suppress_until_ms = 0;
    s_mesh_ignore_press_until_up = false;
}

#ifndef BOOT_KEY_SHUTDOWN_LONG_MS
#define BOOT_KEY_SHUTDOWN_LONG_MS 3000u
#endif

static int s_boot_prev_raw = 0;
static bool s_boot_tracking = false;
static bool s_boot_long_fired = false;
static uint32_t s_boot_press_start_ms = 0;

static bool deckpro_is_main_menu_root(void)
{
    return page_curr == 0 && !w20_shell_mesh_screen_allowed() && scr_mgr_is_root_top() && scr_mgr_top_id() == SCREEN0_ID;
}

extern "C" bool ui_deckpro_shutdown_ui_active(void)
{
    return s_deck_shutdown_ui_active;
}

extern "C" bool ui_deckpro_suppress_mesh_boot_key(int input_event)
{
    /* 关机页进行中：吞掉一切 Boot 相关 Mesh 事件 */
    if (s_deck_shutdown_ui_active) {
        return input_event == INPUT_BROKER_USER_PRESS || input_event == INPUT_BROKER_SELECT ||
               input_event == INPUT_BROKER_SHUTDOWN;
    }
    /* Deck 占屏时禁止 Mesh 长按关机（松手会刷 "Shutting Down..." / 深睡帧盖住 Deck 关机页） */
    if (!w20_shell_mesh_screen_allowed() && input_event == INPUT_BROKER_SHUTDOWN) {
        return true;
    }
    if (!deckpro_is_main_menu_root()) {
        return false;
    }
    return input_event == INPUT_BROKER_USER_PRESS || input_event == INPUT_BROKER_SELECT || input_event == INPUT_BROKER_SHUTDOWN;
}

extern "C" void ui_deckpro_poll_boot_shutdown(void)
{
    if (!deckpro_is_main_menu_root()) {
        s_boot_tracking = false;
        s_boot_long_fired = false;
        s_boot_prev_raw = ui_input_read_boot_key();
        return;
    }

    const int raw = ui_input_read_boot_key();

    if (raw && !s_boot_prev_raw) {
        s_boot_press_start_ms = millis();
        s_boot_tracking = true;
        s_boot_long_fired = false;
    }

    if (raw && s_boot_tracking && !s_boot_long_fired) {
        if (millis() - s_boot_press_start_ms >= BOOT_KEY_SHUTDOWN_LONG_MS) {
            s_boot_long_fired = true;
            W20_UI_LOG("boot long %ums -> SCREEN9 shutdown", (unsigned)BOOT_KEY_SHUTDOWN_LONG_MS);
            scr_push_subscreen(SCREEN9_ID);
        }
    }

    if (!raw && s_boot_prev_raw) {
        s_boot_tracking = false;
        s_boot_long_fired = false;
    }

    s_boot_prev_raw = raw;
}

extern "C" void ui_deckpro_poll_low_batt_shutdown(void)
{
#if defined(W20_MESHNOLOGY)
    static bool s_low_batt_shutdown_pushed = false;
    if (s_low_batt_shutdown_pushed)
        return;

    w20_battery_auto_shutdown_poll();
    if (!w20_battery_auto_shutdown_pending())
        return;

    s_low_batt_shutdown_pushed = true;
    s_shutdown_low_batt_hint = true;
    W20_UI_LOG("low-batt auto -> SCREEN9 (img_start + Low battery off)");
    scr_push_subscreen(SCREEN9_ID);
#else
    (void)0;
#endif
}
#endif

void ui_deckpro_entry(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    disp->theme = lv_theme_mono_init(disp, false, LV_FONT_DEFAULT);

    touch_chk_timer = lv_timer_create(indev_get_gesture_dir, LV_INDEV_DEF_READ_PERIOD, NULL);
    lv_timer_pause(touch_chk_timer);

    taskbar_update_timer = lv_timer_create(menu_taskbar_update_timer_cb, 1000, NULL);
    lv_timer_pause(taskbar_update_timer);

    scr_mgr_init();

    scr_mgr_register(SCREEN0_ID,    &screen0);      // menu
    scr_mgr_register(SCREEN1_ID,    &screen1);      // Lora
    scr_mgr_register(SCREEN1_1_ID,  &screen1_1);    // - Auto send
    scr_mgr_register(SCREEN2_ID,    &screen2);      // Setting
    scr_mgr_register(SCREEN2_1_ID,  &screen2_1);    //  - About System
    scr_mgr_register(SCREEN3_ID,    &screen3);      // 
    scr_mgr_register(SCREEN4_ID,    &screen4);      // WIFI
    scr_mgr_register(SCREEN4_1_ID,  &screen4_1);    //  - WIFI Config
    scr_mgr_register(SCREEN4_2_ID,  &screen4_2);    //  - WIFI Scan
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    scr_mgr_register(SCREEN4_3_ID,  &screen4_3);    //  - WiFi OTA (browser upload)
#endif
    scr_mgr_register(SCREEN5_ID,    &screen5);      // 
    scr_mgr_register(SCREEN6_ID,    &screen6);      // Battery (IP5356 直显)
    scr_mgr_register(SCREEN7_ID,    &screen7);      // 
    scr_mgr_register(SCREEN8_ID,    &screen8);      // A7682E
    scr_mgr_register(SCREEN8_1_ID,  &screen8_1);    //  - Call test
    scr_mgr_register(SCREEN8_2_ID,  &screen8_2);    //  - AT test
    scr_mgr_register(SCREEN9_ID,    &screen9);      // Shutdown
    scr_mgr_register(SCREEN10_ID,   &screen10);     // PCM5102
    scr_mgr_register(SCREEN11_ID,   &screen11);      // eBook TOC
    scr_mgr_register(SCREEN11_1_ID, &screen11_1);    // eBook reader
    scr_mgr_register(SCREEN12_ID,   &screen12);      // Boot logo（early splash 已刷 img_start，不再走此页）

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
    /* early splash 已显示 img_start；直接进主菜单，省掉 SCREEN12 再等 2s */
    scr_mgr_switch(SCREEN0_ID, false);
#else
    scr_mgr_switch(SCREEN12_ID, false); // boot logo, then menu via timer
#endif
    scr_mgr_set_anim(LV_SCR_LOAD_ANIM_OVER_LEFT, LV_SCR_LOAD_ANIM_OVER_LEFT, LV_SCR_LOAD_ANIM_OVER_LEFT);

    // menu_keypad = lv_label_create(lv_layer_top());
    // lv_obj_set_style_text_font(menu_keypad, FONT_BOLD_MONO_SIZE_15, LV_PART_MAIN);
    // lv_label_set_text(menu_keypad, " ");
    // lv_obj_align(menu_keypad, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // menu_timer = lv_timer_create(menu_keypay_get_event, 40, NULL);
}
