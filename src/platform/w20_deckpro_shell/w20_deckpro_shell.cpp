/**
 * Deck Pro LVGL home shell coexisting with Meshtastic (HSPI EPD; Meshtastic SPI = LoRa).
 */
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"
#include "platform/w20_deckpro_shell/w20_lora_arbiter.h"
#include "platform/w20_deckpro_shell/w20_gps_arbiter.h"
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#include "platform/w20_deckpro/w20_wifi_ota.h"
#include "graphics/EInkDisplay2.h"
#include "graphics/Screen.h"
#include "graphics/draw/MenuHandler.h"
#include "graphics/draw/NotificationRenderer.h"
#include "graphics/draw/UIRenderer.h"
#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif
#include "mesh/NodeDB.h"
#include "utilities.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <TouchDrvCST3530.hpp>
#include <Wire.h>
#include <gdeq/GxEPD2_310_DEPG0310BxU253.h>
#include <cstring>
#include <lvgl.h>

#include "factory.h"
#include "peri_lighting_led_key.h"
#include "peripheral.h"
#include "ui_deckpro.h"
#include "ui_deckpro_port.h"
#include "ui_scr_mrg.h"
#include "input/TouchScreenImpl1.h"
#include "platform/w20_deckpro_shell/w20_ui_debug.h"
#include "platform/w20_deckpro/w20_dbg_log.h"
#include "w20_ip5356.h"
#include "w20_power_battery.h"
#include "src/assets.h"
#ifdef MESHCORE_DECK_SHELL
#include "platform/w20_deckpro_shell/w20_meshcore_ui.h"
#endif

extern graphics::Screen *screen;
extern TouchScreenImpl1 *touchScreenImpl1;
/* img_start INDEXED_1BIT：前 8 字节为调色板，其后为 240×320 1bpp */
static constexpr size_t kImgStartPaletteBytes = 8;

/**
 * LVGL →1bpp 帧缓冲 的 Y 映射（与触摸一致）。
 * 1：与 Meshtastic 第二页一致 — 线性 Y（buffer 顶行 = 面板顶，同 EInkDisplay::forceDisplay 未 flip 时）。
 * 0：原厂 w20_cp / factory.ino — Y 镜像 dst_y = H-1-y（旧 Deck 单固件装配）。
 */
#ifndef W20_DECKPRO_FLIP_Y
#define W20_DECKPRO_FLIP_Y 0
#endif

//-----------------------------------------------------------------------------

TouchDrvCST3530 touch;
bool peri_init_st[E_PERI_NUM_MAX] = {false};

static SPIClass deck_epd_spi(HSPI);
using DeckEpd = GxEPD2_BW<EINK_DISPLAY_MODEL, EINK_DISPLAY_MODEL::HEIGHT>;
static DeckEpd deck_epd(EINK_DISPLAY_MODEL(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY));
static bool s_deck_epd_hw_ready = false;

uint8_t *decodebuffer = nullptr;
lv_timer_t *flush_timer = nullptr;
static volatile bool s_epd_flush_done = false;
static volatile bool s_epd_flush_pending = false;
static volatile uint32_t s_epd_flush_gen = 0;
static volatile bool s_epd_flush_suppressed = false;
/** 手指按下期间禁止启动新的 EPD flush，避免「先刷当前页按下态，再刷目标页」。 */
static volatile bool s_touch_hold_epd = false;
/** 子菜单 push/pop 后：墨水未刷完前屏蔽 LVGL 点击，避免「逻辑已进页、屏仍旧页再点穿透」。 */
static volatile bool s_epd_nav_touch_lock = false;
static uint32_t s_epd_nav_touch_lock_ms = 0;
int disp_refr_mode = DISP_REFR_MODE_PART;

static int s_menu_page_index = 0;

/** Mesh→菜单同步刷期间：屏蔽触摸、合并为单次 LVGL+FULL 刷。 */
static bool s_menu_present_active = false;

#if W20_EPD_ASYNC_FLUSH
static SemaphoreHandle_t s_epd_mutex;
static TaskHandle_t s_epd_worker_task;
static uint8_t *s_epd_flush_snapshot = nullptr;
static size_t s_epd_flush_snapshot_bytes = 0;
static volatile bool s_epd_job_full = false;
static uint32_t s_epd_worker_job_gen = 0;
#endif

static void w20_deckpro_epd_flush_if_pending(void);
static void w20_deckpro_epd_do_flush(const uint8_t *bitmap, lv_coord_t w, lv_coord_t h, bool full, uint32_t *elapsed_ms);
static void w20_deckpro_epd_async_init(void);


static bool s_epd_reuse_ready = false;

/** 进 Mesh 时 forceDisplay(true) 已全刷；避免 runOnce 在 FIXED 时再刷一次。 */
static bool s_skip_mesh_idle_force = false;
/** 唤醒后延时清除 skip（millis 截止）。 */
static uint32_t s_wake_epd_skip_until_ms = 0;

/** 手势回调里只置位，在 shell_loop 下一轮做 EPD/I2C，避免与触摸抢总线。 */
static bool s_mesh_present_pending = false;

/** 主菜单后台已预构建 Meshtastic UI（setFrames + 首帧 buffer）。 */
static bool s_mesh_ui_prewarmed = false;
static bool s_mesh_prewarm_active = false;
static uint32_t s_mesh_prewarm_due_ms = 0;

/** 为 true 时 EInkDisplay::forceDisplay 一律跳过，直到允许单次 flush。 */
static bool s_mesh_epd_suppress = false;
static bool s_mesh_epd_allow_once = false;
/** overlay 菜单操作期间：屏蔽 runOnce/异步路径的 forceDisplay，仅 flush_once 一次。 */
static bool s_mesh_overlay_epd_gate = false;

static void w20_shell_process_mesh_enter_pending(void);
static void w20_shell_prewarm_mesh_ui_impl(void);
static void w20_shell_ensure_welcome_before_mesh_flush(void);

bool w20_shell_mesh_ui_prewarm_active(void)
{
    return s_mesh_prewarm_active;
}

void w20_shell_prewarm_mesh_ui(void)
{
    w20_shell_prewarm_mesh_ui_impl();
}

bool w20_deckpro_shell_epd_reuse_ready(void)
{
    return s_epd_reuse_ready;
}

void *w20_deckpro_shell_epd_gx_bw_ptr(void)
{
    return s_epd_reuse_ready ? (void *)&deck_epd : nullptr;
}

bool w20_shell_mesh_screen_allowed(void)
{
    return s_menu_page_index >= 1;
}

bool w20_shell_skip_mesh_idle_force(void)
{
    if (s_wake_epd_skip_until_ms != 0) {
        if ((int32_t)(millis() - s_wake_epd_skip_until_ms) < 0) {
            return true;
        }
        s_wake_epd_skip_until_ms = 0;
        s_skip_mesh_idle_force = false;
    }
    return s_skip_mesh_idle_force;
}

void w20_shell_set_skip_mesh_idle_force(bool skip)
{
    s_skip_mesh_idle_force = skip;
    if (!skip) {
        s_wake_epd_skip_until_ms = 0;
    }
}

void w20_shell_arm_wake_epd_skip_ms(uint32_t ms)
{
    s_skip_mesh_idle_force = true;
    s_wake_epd_skip_until_ms = millis() + ms;
}

bool w20_shell_mesh_epd_may_update(void)
{
    if (s_mesh_overlay_epd_gate && !s_mesh_epd_allow_once) {
        return false;
    }
    if (!s_mesh_epd_suppress)
        return true;
    if (s_mesh_epd_allow_once) {
        s_mesh_epd_allow_once = false;
        return true;
    }
    return false;
}

void w20_shell_mesh_overlay_epd_begin(void)
{
    s_mesh_overlay_epd_gate = true;
    s_skip_mesh_idle_force = true;
}

void w20_shell_mesh_overlay_epd_end(void)
{
    s_mesh_overlay_epd_gate = false;
    s_skip_mesh_idle_force = false;
}

void w20_shell_mesh_epd_flush_once(void)
{
    if (screen == nullptr) {
        return;
    }
    s_mesh_epd_allow_once = true;
    if (auto *eink = static_cast<EInkDisplay *>(screen->getDisplayDevice())) {
        eink->forceDisplay(0);
    }
}

void w20_deckpro_epd_after_refresh(void)
{
#if W20_DECKPRO_EPD_HOLD_IMAGE
    deck_epd.powerOff();
#else
    deck_epd.hibernate();
#endif
}

void w20_deckpro_epd_lock(void)
{
#if W20_EPD_ASYNC_FLUSH
    if (s_epd_mutex != nullptr) {
        xSemaphoreTake(s_epd_mutex, portMAX_DELAY);
    }
#endif
}

void w20_deckpro_epd_unlock(void)
{
#if W20_EPD_ASYNC_FLUSH
    if (s_epd_mutex != nullptr) {
        xSemaphoreGive(s_epd_mutex);
    }
#endif
}

void w20_deckpro_epd_drain(void)
{
#if W20_EPD_ASYNC_FLUSH
    if (s_epd_worker_task == nullptr) {
        return;
    }
    const uint32_t t0 = millis();
    while (millis() - t0 < 10000) {
        w20_deckpro_epd_flush_if_pending();
        w20_wifi_ota_loop();
        if (s_epd_flush_done && !s_epd_flush_pending) {
            break;
        }
        delay(10);
    }
    w20_deckpro_epd_lock();
    w20_deckpro_epd_unlock();
#endif
}

static void w20_shell_set_mesh_touch_input(bool mesh_page)
{
    (void)mesh_page;
    /* 全程禁用 TouchScreenImpl1：主菜单 LVGL、Meshtastic 页 poll_gesture 直接读 CST3530。 */
    if (touchScreenImpl1)
        touchScreenImpl1->disable();
}

/** 刷 EPD 前重建 Welcome（prewarm 可能已置 overlay 但未上屏，首滑须与 flush 同帧）。 */
static void w20_shell_ensure_welcome_before_mesh_flush(void)
{
#ifndef DISABLE_WELCOME_UNSET
    if (config.lora.region != meshtastic_Config_LoRaConfig_RegionCode_UNSET)
        return;
    graphics::NotificationRenderer::resetBanner();
    graphics::menuHandler::OnboardMessage();
    W20_UI_LOG("mesh welcome overlay (region UNSET) before EPD flush");
#endif
}

static void w20_shell_prewarm_mesh_ui_impl(void)
{
#ifdef MESHCORE_DECK_SHELL
    /* MeshCore UI 在进入 Mesh 页时再 begin，无需 Meshtastic 式预构建帧。 */
    s_mesh_ui_prewarmed = true;
    return;
#else
    if (s_mesh_ui_prewarmed || s_mesh_prewarm_active || screen == nullptr || w20_deck_sleep_active())
        return;
    if (s_menu_page_index >= 1)
        return;

    const uint32_t t0 = millis();
    W20_UI_LOG("mesh_prewarm start");
    s_mesh_prewarm_active = true;
    s_mesh_epd_suppress = true;
    s_skip_mesh_idle_force = true;

    graphics::UIRenderer::w20NotifyMeshtasticPageShown();
    screen->stopBootScreen();
    screen->w20PrepareMeshUiFrame();

    s_skip_mesh_idle_force = false;
    s_mesh_epd_suppress = false;
    s_mesh_prewarm_active = false;
    s_mesh_ui_prewarmed = true;
    W20_UI_LOG("mesh_prewarm done %lums", (unsigned long)(millis() - t0));
#endif
}

static void w20_shell_process_mesh_enter_pending(void)
{
    if (!s_mesh_present_pending)
        return;
    s_mesh_present_pending = false;

#ifdef MESHCORE_DECK_SHELL
    ui_deckpro_gesture_reset();
    s_skip_mesh_idle_force = true;
    const uint32_t t0 = millis();
    w20_meshcore_ui_on_enter();
    s_mesh_ui_prewarmed = true;
    s_skip_mesh_idle_force = false;
    W20_UI_LOG("meshcore_present_on_epd done %lums", (unsigned long)(millis() - t0));
    return;
#else
    if (screen == nullptr)
        return;

    ui_deckpro_gesture_reset();
    s_skip_mesh_idle_force = true;
    s_mesh_epd_suppress = true;
    s_mesh_epd_allow_once = false;

    graphics::UIRenderer::w20NotifyMeshtasticPageShown();
    /* 已 prewarm 时帧列表已建好，勿再 stopBootScreen()，否则 runOnce 会重复 setFrames + forceDisplay。 */
    if (!s_mesh_ui_prewarmed) {
        screen->stopBootScreen();
    }

    const uint32_t t0 = millis();
    const bool had_prewarm = s_mesh_ui_prewarmed;
    w20_shell_ensure_welcome_before_mesh_flush();
    /* 排空 cmdQueue、重绘 UI；末尾 EPD 由 suppress 挡住，下面只 flush 一次。 */
    screen->forceDisplay(true);
    s_mesh_ui_prewarmed = true;

    w20_shell_mesh_epd_flush_once();

    /* 消化菜单页积压的 STOP_BOOT_SCREEN，避免清 skip 后 Screen 线程再 setFrames+全刷。 */
    screen->w20DrainPendingCmds();

    s_mesh_epd_suppress = false;
    s_skip_mesh_idle_force = false;
    W20_UI_LOG("mesh_present_on_epd done %lums (prewarmed=%d)", (unsigned long)(millis() - t0), had_prewarm ? 1 : 0);
#endif
}

void w20_shell_on_menu_page(int page_index)
{
    const int prev = s_menu_page_index;
    W20_UI_LOG("on_menu_page %d -> %d (mesh_allowed=%d)", prev, page_index, page_index >= 1 ? 1 : 0);
    w20_shell_set_mesh_touch_input(page_index >= 1);
    if (page_index >= 1) {
        ui_deckpro_pause_aux_timers();
        ui_deckpro_gesture_reset();
        s_menu_page_index = page_index;
        s_mesh_present_pending = true;
    } else {
        s_menu_page_index = page_index;
        s_mesh_present_pending = false;
    }
    if (page_index >= 1) {
        w20_lora_arbiter_request_mesh();
        w20_gps_arbiter_request_mesh();
    }
    if (page_index == 0) {
#ifdef MESHCORE_DECK_SHELL
        w20_meshcore_ui_on_exit();
#endif
        s_skip_mesh_idle_force = false;
        ui_deckpro_resume_aux_timers();
        ui_deckpro_refresh_taskbar_battery();
        ui_deckpro_refresh_taskbar_time();
        w20_deckpro_present_menu_on_epd();
    }
}

void w20_deckpro_present_menu_on_epd(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp == nullptr || decodebuffer == nullptr) {
        return;
    }

    ui_deckpro_pause_aux_timers();
    factory_pause_flush_timer();
    disp_refr_mode = DISP_REFR_MODE_FULL;
    s_epd_flush_done = false;
    s_menu_present_active = true;
    W20_UI_LOG("menu_present_on_epd start");

    lv_obj_t *scr = lv_scr_act();
    if (scr != nullptr) {
        ui_deckpro_menu_prepare_for_epd();
        lv_obj_update_layout(scr);
        lv_obj_invalidate(scr);
    }

    const uint32_t t0 = millis();
    while (millis() - t0 < 8000 && !s_epd_flush_done) {
        lv_task_handler();
        w20_deckpro_epd_flush_if_pending();
        w20_wifi_ota_loop();
        delay(5);
    }

    s_menu_present_active = false;
    factory_pause_flush_timer();
    ui_deckpro_resume_aux_timers();
    W20_UI_LOG("menu_present_on_epd done %lums", (unsigned long)(millis() - t0));
}

//-----------------------------------------------------------------------------

static bool ink_screen_init_hsipi(void)
{
    if (s_deck_epd_hw_ready) {
        return true;
    }
    pinMode(BOARD_EPD_CS, OUTPUT);
    digitalWrite(BOARD_EPD_CS, HIGH);
    /* EPD is write-only; pass MOSI as dummy MISO to avoid HSPI "no default pins" log on S3 */
    deck_epd_spi.begin(BOARD_SPI_SCK, BOARD_SPI_MOSI, BOARD_SPI_MOSI, -1);
    /* initial=false：避免白屏空刷；由 early_splash / 菜单 FULL 出第一帧 */
    deck_epd.init(0, false, 40, false, deck_epd_spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    /* 0/1/2/3 = 0°/90°/180°/270°；与 FLIP_Y 二选一调试，避免叠用导致方向混乱 */
    deck_epd.setRotation(0);
    deck_epd.setPartialWindow(0, 0, EINK_WIDTH, EINK_HEIGHT);
    s_deck_epd_hw_ready = true;
    return true;
}

void w20_deckpro_early_splash(void)
{
    const uint32_t t0 = millis();
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    pinMode(BOARD_EPD_CS, OUTPUT);
    digitalWrite(BOARD_EPD_CS, HIGH);

    if (!ink_screen_init_hsipi()) {
        Serial.println("[W20] early splash: EPD init fail");
        return;
    }

    /* INDEXED_1BIT → 跳过调色板；方向须与 disp_flush 一致（FLIP_Y=0 时 Y 镜像） */
    const int16_t w = (int16_t)img_start.header.w;
    const int16_t h = (int16_t)img_start.header.h;
    const uint8_t *bmp = img_start.data + kImgStartPaletteBytes;
    const int row_bytes = (w + 7) / 8;

    deck_epd.setFullWindow();
    deck_epd.firstPage();
    do {
        deck_epd.fillScreen(GxEPD_WHITE);
#if W20_DECKPRO_FLIP_Y
        deck_epd.drawInvertedBitmap(0, 0, bmp, w, h, GxEPD_BLACK);
#else
        for (int16_t y = 0; y < h; y++) {
            deck_epd.drawInvertedBitmap(0, h - 1 - y, bmp + (int32_t)y * row_bytes, w, 1, GxEPD_BLACK);
        }
#endif
    } while (deck_epd.nextPage());
    w20_deckpro_epd_after_refresh();

    Serial.printf("[W20] early splash img_start done %lums\n", (unsigned long)(millis() - t0));
}

void w20_deckpro_sleep_prompt_flush(void)
{
    if (!s_deck_epd_hw_ready) {
        return;
    }
    /* 清触摸 hold / 停墨；同步刷，避免 async 超时导致屏不动 */
    s_touch_hold_epd = false;
    s_epd_flush_suppressed = false;
    factory_pause_flush_timer();
    s_epd_flush_pending = false;
    s_epd_flush_done = true;
    w20_deckpro_epd_drain();

    const uint32_t t0 = millis();
    lv_obj_t *parent = lv_scr_act();
    if (parent == nullptr || decodebuffer == nullptr) {
        /* 无 LVGL：仅刷 img_start（与 early splash 同向） */
        const int16_t w = (int16_t)img_start.header.w;
        const int16_t h = (int16_t)img_start.header.h;
        const uint8_t *bmp = img_start.data + kImgStartPaletteBytes;
        const int row_bytes = (w + 7) / 8;
        deck_epd.setFullWindow();
        deck_epd.firstPage();
        do {
            deck_epd.fillScreen(GxEPD_WHITE);
#if W20_DECKPRO_FLIP_Y
            deck_epd.drawInvertedBitmap(0, 0, bmp, w, h, GxEPD_BLACK);
#else
            for (int16_t y = 0; y < h; y++) {
                deck_epd.drawInvertedBitmap(0, h - 1 - y, bmp + (int32_t)y * row_bytes, w, 1, GxEPD_BLACK);
            }
#endif
        } while (deck_epd.nextPage());
        w20_deckpro_epd_after_refresh();
        s_epd_flush_done = true;
        W20_UI_LOG("sleep_prompt img_start fallback %lums", (unsigned long)(millis() - t0));
        return;
    }

    /* 开机 logo 全屏 + 下方 Sleep 文案（与 Shutdown/SCREEN12 同源 img_start） */
    lv_obj_t *ov = lv_obj_create(parent);
    lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
    lv_obj_align(ov, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ov, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ov, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *logo = lv_img_create(ov);
    lv_img_set_src(logo, &img_start);
    lv_obj_set_style_border_width(logo, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(logo, 0, LV_PART_MAIN);
    lv_obj_align(logo, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *title = lv_label_create(ov);
    lv_label_set_text(title, "Sleep");
    lv_obj_set_style_text_color(title, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &Font_Mono_Bold_19, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_BOTTOM_MID, 0, -36);

    lv_obj_t *hint = lv_label_create(ov);
    lv_label_set_text(hint, "Press BOOT to wake");
    lv_obj_set_style_text_color(hint, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &Font_Mono_Bold_14, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_obj_update_layout(ov);
    lv_obj_invalidate(ov);
    lv_refr_now(lv_disp_get_default());
    factory_pause_flush_timer();
    s_epd_flush_pending = false;
    s_epd_flush_gen++;

    uint32_t elapsed = 0;
    w20_deckpro_epd_lock();
    w20_deckpro_epd_do_flush(decodebuffer, LV_HOR_RES, LV_VER_RES, true, &elapsed);
    w20_deckpro_epd_unlock();
    s_epd_flush_done = true;
    s_epd_flush_pending = false;

    lv_obj_del(ov);
    W20_UI_LOG("sleep_prompt img_start+Sleep %lums (epd=%lums)", (unsigned long)(millis() - t0),
               (unsigned long)elapsed);
}

static void w20_deckpro_epd_do_flush(const uint8_t *bitmap, lv_coord_t w, lv_coord_t h, bool full, uint32_t *elapsed_ms)
{
    const uint32_t t0 = millis();
    if (full) {
        deck_epd.setFullWindow();
    } else {
        deck_epd.setPartialWindow(0, 0, w, h);
    }
    deck_epd.firstPage();
    const lv_coord_t draw_h = full ? h : (h - 3);
    do {
        deck_epd.drawInvertedBitmap(0, 0, bitmap, w, draw_h, GxEPD_BLACK);
    } while (deck_epd.nextPage());
    w20_deckpro_epd_after_refresh();
    if (elapsed_ms != nullptr) {
        *elapsed_ms = millis() - t0;
    }
}

#if W20_EPD_ASYNC_FLUSH
static void epd_flush_worker_task(void *param)
{
    (void)param;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            const bool full = s_epd_job_full;
            const uint32_t job_gen = s_epd_worker_job_gen;
            uint32_t elapsed = 0;

            w20_deckpro_epd_lock();
            if (s_epd_flush_snapshot != nullptr) {
                w20_deckpro_epd_do_flush(s_epd_flush_snapshot, LV_HOR_RES, LV_VER_RES, full, &elapsed);
            }
            w20_deckpro_epd_unlock();

            W20_UI_LOG("epd_flush %s %dx%d %lums epd_end=%s async=1", full ? "FULL" : "PART", (int)LV_HOR_RES,
                       (int)LV_VER_RES, (unsigned long)elapsed,
#if W20_DECKPRO_EPD_HOLD_IMAGE
                       "powerOff");
#else
                       "hibernate");
#endif

            if (disp_refr_mode != DISP_REFR_MODE_FULL) {
                disp_refr_mode = DISP_REFR_MODE_PART;
            }
            if (flush_timer != nullptr) {
                lv_timer_pause(flush_timer);
            }
            if (job_gen == s_epd_flush_gen) {
                s_epd_flush_done = true;
                s_epd_nav_touch_lock = false;
            }

            if (!s_epd_flush_pending) {
                break;
            }
            s_epd_flush_pending = false;
            s_epd_job_full = (disp_refr_mode == DISP_REFR_MODE_FULL);
            s_epd_worker_job_gen = s_epd_flush_gen;
            if (decodebuffer != nullptr && s_epd_flush_snapshot != nullptr && s_epd_flush_snapshot_bytes > 0) {
                memcpy(s_epd_flush_snapshot, decodebuffer, s_epd_flush_snapshot_bytes);
            }
        }
    }
}

static void w20_deckpro_epd_async_init(void)
{
    if (s_epd_worker_task != nullptr) {
        return;
    }
    s_epd_mutex = xSemaphoreCreateMutex();
    if (s_epd_mutex == nullptr) {
        return;
    }
    const size_t pixels = (size_t)LCD_HOR_SIZE * (size_t)LCD_VER_SIZE;
    s_epd_flush_snapshot_bytes = (pixels + 7) / 8;
    s_epd_flush_snapshot = (uint8_t *)ps_calloc(1, s_epd_flush_snapshot_bytes);
    if (s_epd_flush_snapshot == nullptr) {
        static uint8_t s_epd_flush_snapshot_static[(LCD_HOR_SIZE * LCD_VER_SIZE + 7) / 8];
        s_epd_flush_snapshot = s_epd_flush_snapshot_static;
    }
    xTaskCreate(epd_flush_worker_task, "epdFlush", 8192, nullptr, 2, &s_epd_worker_task);
    W20_UI_LOG("epd async flush task started");
}
#endif

static void w20_deckpro_epd_flush_if_pending(void)
{
    /* 仅上传中禁刷；空闲监听时仍要能刷出 Server ready / URL */
    if (w20_wifi_ota_is_busy()) {
        return;
    }
    if (s_epd_flush_suppressed || s_touch_hold_epd) {
        s_epd_flush_pending = false;
        return;
    }
    if (!s_epd_flush_pending) {
        return;
    }
    s_epd_flush_pending = false;

    lv_coord_t w = LV_HOR_RES;
    lv_coord_t h = LV_VER_RES;
    const bool full = (disp_refr_mode == DISP_REFR_MODE_FULL);
    if (full) {
        disp_refr_mode = DISP_REFR_MODE_PART;
    }
    const uint32_t job_gen = s_epd_flush_gen;

#if W20_EPD_ASYNC_FLUSH
    if (s_epd_worker_task == nullptr || s_epd_flush_snapshot == nullptr) {
        w20_deckpro_epd_lock();
        uint32_t elapsed = 0;
        w20_deckpro_epd_do_flush(decodebuffer, w, h, full, &elapsed);
        w20_deckpro_epd_unlock();
        W20_UI_LOG("epd_flush %s %dx%d %lums epd_end=%s async=0", full ? "FULL" : "PART", (int)w, (int)h,
                   (unsigned long)elapsed,
#if W20_DECKPRO_EPD_HOLD_IMAGE
                   "powerOff");
#else
                   "hibernate");
#endif
        disp_refr_mode = DISP_REFR_MODE_PART;
        if (flush_timer != nullptr) {
            lv_timer_pause(flush_timer);
        }
        if (job_gen == s_epd_flush_gen) {
            s_epd_flush_done = true;
            s_epd_nav_touch_lock = false;
        }
        return;
    }

    s_epd_job_full = full;
    s_epd_worker_job_gen = s_epd_flush_gen;
    if (decodebuffer != nullptr && s_epd_flush_snapshot_bytes > 0) {
        memcpy(s_epd_flush_snapshot, decodebuffer, s_epd_flush_snapshot_bytes);
    }
    xTaskNotifyGive(s_epd_worker_task);
    return;
#else
    w20_deckpro_epd_lock();
    uint32_t elapsed = 0;
    w20_deckpro_epd_do_flush(decodebuffer, w, h, full, &elapsed);
    w20_deckpro_epd_unlock();

    W20_UI_LOG("epd_flush %s %dx%d %lums epd_end=%s", full ? "FULL" : "PART", (int)w, (int)h,
               (unsigned long)elapsed,
#if W20_DECKPRO_EPD_HOLD_IMAGE
               "powerOff");
#else
               "hibernate");
#endif

    disp_refr_mode = DISP_REFR_MODE_PART;
    if (flush_timer != nullptr) {
        lv_timer_pause(flush_timer);
    }
    if (job_gen == s_epd_flush_gen) {
        s_epd_flush_done = true;
        s_epd_nav_touch_lock = false;
    }
#endif
}

static void flush_timer_cb(lv_timer_t *t)
{
    (void)t;
    lv_disp_t *disp = lv_disp_get_default();
    if (disp->rendering_in_progress == false) {
        s_epd_flush_pending = true;
        lv_timer_pause(flush_timer);
    }
}

static void dips_render_start_cb(lv_disp_drv_t *disp_drv)
{
    (void)disp_drv;
    if (w20_deck_sleep_active()) {
        return;
    }
    /* 仅固件上传中禁墨水；OTA 空闲监听必须能刷「Server ready」URL */
    if (w20_wifi_ota_is_busy()) {
        return;
    }
    /* SoftAP 配网等：允许 LVGL 改 buffer，但禁止启动 EPD flush（否则 pause 会被 resume 掉）。 */
    if (s_epd_flush_suppressed) {
        return;
    }
    /* 触摸未抬起：只让 LVGL 画到 buffer，不启动墨水 flush（导航/开关都等抬手后再刷一次）。 */
    if (s_touch_hold_epd) {
        factory_pause_flush_timer();
        s_epd_flush_pending = false;
        return;
    }
    if (s_menu_present_active) {
        /* 菜单同步刷期间只允许一次 FULL 刷墨，避免 async 下 s_epd_flush_gen 连增导致 flush_done 永不置位。 */
        if (s_epd_flush_done || s_epd_flush_pending) {
            return;
        }
    }
    s_epd_flush_done = false;
    s_epd_flush_pending = false;
    s_epd_flush_gen++;
    static uint32_t s_render_seq = 0;
    s_render_seq++;
    W20_UI_LOG("lv_render_start #%lu full_refresh=1", (unsigned long)s_render_seq);
    /*
     * 与 w20_cp factory.ino 一致：不在 render_start 整帧 memset。
     * 每帧清 0xFF 会让 LVGL 尚未画完时整屏先被刷成「空底」，子菜单切换易闪一帧；
     * 首帧白底仍在 lvgl_init_shell 里 calloc 后对 decodebuffer 做一次 memset(0xFF)。
     * GxEPD2 drawInvertedBitmap：MSB=0 黑、MSB=1 留白；勿用 memset(0) 作底。
     */
    if (flush_timer == nullptr) {
        flush_timer = lv_timer_create(flush_timer_cb, 10, nullptr);
    } else {
        lv_timer_resume(flush_timer);
    }
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t x2 = area->x2 > (LCD_HOR_SIZE - 1) ? (LCD_HOR_SIZE - 1) : area->x2;
    int32_t y2 = area->y2 > (LCD_VER_SIZE - 1) ? (LCD_VER_SIZE - 1) : area->y2;

    uint32_t w = (x2 >= x1) ? (uint32_t)(x2 - x1 + 1) : 0;
    uint32_t h = (y2 >= y1) ? (uint32_t)(y2 - y1 + 1) : 0;

    if (w == 0 || h == 0 || decodebuffer == nullptr) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    size_t dbuf_bytes = ((size_t)LCD_HOR_SIZE * LCD_VER_SIZE + 7) / 8;

    for (uint32_t row = 0; row < h; ++row) {
#if W20_DECKPRO_FLIP_Y
        const uint32_t dst_y = (uint32_t)(y1 + row);
#else
        const uint32_t dst_y = (LCD_VER_SIZE - 1) - (y1 + row);
#endif
        for (uint32_t col = 0; col < w; ++col) {
            size_t px = (size_t)dst_y * LCD_HOR_SIZE + (size_t)(x1 + col);
            size_t buf_idx = px / 8;
            uint8_t bit = 7 - (px % 8);

            if (buf_idx < dbuf_bytes) {
                uint32_t src_idx = row * w + col;
                uint8_t v = (color_p[src_idx].full ? 1 : 0);
                if (v)
                    decodebuffer[buf_idx] |= (1u << bit);
                else
                    decodebuffer[buf_idx] &= ~(1u << bit);
            }
        }
    }

    lv_disp_flush_ready(disp_drv);
}

#ifndef W20_EPD_NAV_TOUCH_LOCK_MAX_MS
/** 导航触控锁上限：须 ≥ FULL 刷墨（约 1.3s）；超时兜底，避免异常卡死触摸。 */
#define W20_EPD_NAV_TOUCH_LOCK_MAX_MS 2500u
#endif

static bool w20_epd_nav_touch_lock_active(void)
{
    if (!s_epd_nav_touch_lock) {
        return false;
    }
    /* 本帧 FULL 已刷完则解锁（worker / sync flush 置 flush_done） */
    if (s_epd_flush_done && !s_epd_flush_pending) {
        s_epd_nav_touch_lock = false;
        return false;
    }
    /* 停墨时不会 flush_done，勿死锁 */
    if (s_epd_flush_suppressed) {
        s_epd_nav_touch_lock = false;
        return false;
    }
    if ((millis() - s_epd_nav_touch_lock_ms) > W20_EPD_NAV_TOUCH_LOCK_MAX_MS) {
        s_epd_nav_touch_lock = false;
        return false;
    }
    return true;
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    static bool s_log_down = false;

    /* 导航后锁到本帧 FULL 刷完：FULL≈1.3s，旧 500ms 会提前放行 → 眼看旧页、LVGL 已是新页而误点 */
    const bool suppress_lvgl =
        s_menu_present_active || ui_deckpro_lvgl_touch_suppressed() || w20_epd_nav_touch_lock_active();
    uint8_t touched = touch.getPoint(&last_x, &last_y, 1);
    if (suppress_lvgl) {
        data->state = LV_INDEV_STATE_REL;
        data->point.x = last_x;
#if W20_DECKPRO_FLIP_Y
        data->point.y = (lv_coord_t)((LCD_VER_SIZE - 1) - last_y);
#else
        data->point.y = last_y;
#endif
        if (touched && !s_log_down) {
            s_log_down = true;
            W20_TOUCH_LOG("touch DOWN raw(%d,%d) lv(%d,%d) flip_y=%d (lvgl suppressed)\n", (int)last_x, (int)last_y,
                       (int)data->point.x, (int)data->point.y, W20_DECKPRO_FLIP_Y);
        } else if (!touched && s_log_down) {
            s_log_down = false;
            W20_TOUCH_LOG("touch UP   raw(%d,%d) lv(%d,%d) (lvgl suppressed)\n", (int)last_x, (int)last_y, (int)data->point.x,
                       (int)data->point.y);
        }
        return;
    }
    if (touched) {
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    data->point.x = last_x;
#if W20_DECKPRO_FLIP_Y
    data->point.y = (lv_coord_t)((LCD_VER_SIZE - 1) - last_y);
#else
    data->point.y = last_y;
#endif

    if (touched && !s_log_down) {
        s_log_down = true;
        s_touch_hold_epd = true;
        factory_pause_flush_timer();
        s_epd_flush_pending = false;
        W20_TOUCH_LOG("touch DOWN raw(%d,%d) lv(%d,%d) flip_y=%d\n", (int)last_x, (int)last_y, (int)data->point.x, (int)data->point.y,
                   W20_DECKPRO_FLIP_Y);
    } else if (!touched && s_log_down) {
        s_log_down = false;
        s_touch_hold_epd = false;
        W20_TOUCH_LOG("touch UP   raw(%d,%d) lv(%d,%d)\n", (int)last_x, (int)last_y, (int)data->point.x, (int)data->point.y);
    }
}

static void lvgl_init_shell(void)
{
    lv_init();

    static lv_disp_draw_buf_t draw_buf_dsc_1;
    lv_color_t *lv_buf = (lv_color_t *)ps_calloc(sizeof(lv_color_t), LCD_HOR_SIZE * 4);
    if (lv_buf == nullptr) {
        lv_buf = (lv_color_t *)ps_calloc(sizeof(lv_color_t), LCD_HOR_SIZE * 1);
        if (lv_buf == nullptr) {
            static lv_color_t lv_buf_static_fallback[LCD_HOR_SIZE * 1];
            lv_buf = lv_buf_static_fallback;
            lv_disp_draw_buf_init(&draw_buf_dsc_1, lv_buf, nullptr, LCD_HOR_SIZE * 1);
        } else {
            lv_disp_draw_buf_init(&draw_buf_dsc_1, lv_buf, nullptr, LCD_HOR_SIZE * 1);
        }
    } else {
        lv_disp_draw_buf_init(&draw_buf_dsc_1, lv_buf, nullptr, LCD_HOR_SIZE * 4);
    }

    size_t pixels = (size_t)LCD_HOR_SIZE * (size_t)LCD_VER_SIZE;
    size_t dbuf_bytes = (pixels + 7) / 8;
    decodebuffer = (uint8_t *)ps_calloc(1, dbuf_bytes);
    if (decodebuffer == nullptr) {
        static uint8_t decodebuffer_static[(LCD_HOR_SIZE * LCD_VER_SIZE + 7) / 8];
        decodebuffer = decodebuffer_static;
    }
    /* calloc 为 0=黑底，与 GxEPD2 Inverted 语义相反；首帧前须为 0xFF 白底 */
    memset(decodebuffer, 0xFF, dbuf_bytes);

    w20_deckpro_epd_async_init();

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_HOR_SIZE;
    disp_drv.ver_res = LCD_VER_SIZE;
    disp_drv.flush_cb = disp_flush;
    disp_drv.render_start_cb = dips_render_start_cb;
    disp_drv.draw_buf = &draw_buf_dsc_1;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

//-----------------------------------------------------------------------------

void disp_full_refr(void)
{
    static uint32_t s_full_seq = 0;
    s_full_seq++;
    W20_UI_LOG("disp_full_refr #%lu", (unsigned long)s_full_seq);
    disp_refr_mode = DISP_REFR_MODE_FULL;
}

void factory_pause_flush_timer(void)
{
    if (flush_timer != nullptr)
        lv_timer_pause(flush_timer);
}

void factory_resume_flush_timer(void)
{
    if (s_epd_flush_suppressed) {
        return;
    }
    if (flush_timer != nullptr)
        lv_timer_resume(flush_timer);
}

void factory_set_epd_flush_suppressed(bool suppressed)
{
    s_epd_flush_suppressed = suppressed;
    if (suppressed) {
        s_epd_flush_pending = false;
        /* SoftAP 等停墨期间不会再 flush，勿一直锁触摸 */
        s_epd_nav_touch_lock = false;
        if (flush_timer != nullptr) {
            lv_timer_pause(flush_timer);
        }
    } else if (flush_timer != nullptr) {
        lv_timer_resume(flush_timer);
    }
}

bool factory_epd_flush_suppressed(void)
{
    return s_epd_flush_suppressed;
}

void ui_deckpro_epd_nav_hold(bool hold)
{
    s_touch_hold_epd = hold;
    if (hold) {
        factory_pause_flush_timer();
        s_epd_flush_pending = false;
    }
}

bool ui_deckpro_epd_nav_touch_locked(void)
{
    return w20_epd_nav_touch_lock_active();
}

bool ui_deckpro_epd_flush_busy(void)
{
    return s_epd_flush_pending || !s_epd_flush_done;
}

void ui_deckpro_epd_prepare_subscreen(void)
{
    factory_pause_flush_timer();
    s_epd_flush_pending = false;
    /* 作废按下期间可能已排队/在途的旧页 PART，只保留随后这一次 FULL */
    s_epd_flush_gen++;
    s_epd_flush_done = false;
    s_touch_hold_epd = false;
    /* 导航后锁触摸至本帧 FULL 刷完；停墨时不锁（否则永远解不开） */
    if (!s_epd_flush_suppressed) {
        s_epd_nav_touch_lock = true;
        s_epd_nav_touch_lock_ms = millis();
    } else {
        s_epd_nav_touch_lock = false;
    }
    if (scr_mgr_top_id() == SCREEN0_ID) {
        ui_deckpro_menu_prepare_for_epd();
    }
    /* 子菜单 push/pop：invalidate 当前屏，避免 EPD 仍显示上一页。 */
    lv_obj_t *scr = lv_scr_act();
    if (scr != nullptr) {
        lv_obj_invalidate(scr);
    }
    disp_full_refr();
}

void factory_epd_hibernate(void)
{
    w20_deckpro_epd_drain();
    w20_deckpro_epd_lock();
    deck_epd.hibernate();
    w20_deckpro_epd_unlock();
    Serial.println("W20 shutdown: deck_epd hibernate (UC8253 0x07)");
}

void factory_epd_hold_image(void)
{
    w20_deckpro_epd_drain();
    w20_deckpro_epd_lock();
    deck_epd.powerOff();
    w20_deckpro_epd_unlock();
    Serial.println("W20 sleep: deck_epd powerOff (hold image, no 0x07)");
}

void factory_epd_release_bus(void)
{
    deck_epd_spi.end();
    s_deck_epd_hw_ready = false;
}

bool factory_epd_resume(void)
{
    return ink_screen_init_hsipi();
}

//-----------------------------------------------------------------------------

void w20_deckpro_shell_init(void)
{
    s_epd_reuse_ready = false;
    if (screen != nullptr) {
        OLEDDisplay *oled = screen->getDisplayDevice();
        if (oled != nullptr) {
            auto *eink = static_cast<EInkDisplay *>(oled);
            eink->hibernateHardware();
        }
    }

    gpio_hold_dis((gpio_num_t)BOARD_LORA_EN);
    gpio_hold_dis((gpio_num_t)BOARD_GPS_EN);
    gpio_deep_sleep_hold_dis();

    if (PIN_RGB_R >= 0) {
        pinMode(PIN_RGB_R, OUTPUT);
        digitalWrite(PIN_RGB_R, LOW);
    }
    if (PIN_RGB_B >= 0) {
        pinMode(PIN_RGB_B, OUTPUT);
        digitalWrite(PIN_RGB_B, LOW);
    }
    if (PIN_LIGHTING_LAMP >= 0) {
        pinMode(PIN_LIGHTING_LAMP, OUTPUT);
        digitalWrite(PIN_LIGHTING_LAMP, LIGHTING_LAMP_OFF_LEVEL);
    }

    pinMode(BOARD_GPS_EN, OUTPUT);
    if (BOARD_GPS_WAKE >= 0) {
        pinMode(BOARD_GPS_WAKE, OUTPUT);
        digitalWrite(BOARD_GPS_WAKE, HIGH);
    }
    digitalWrite(BOARD_LORA_EN, HIGH);
    digitalWrite(BOARD_GPS_EN, HIGH);

    /*
     * Semtech HAL already owns SPI CS after radio_init(). pinMode(CS/RST) here
     * steals the pin from SPI2 and LR2021 version becomes 255.255.
     */
#if !defined(W20_USE_SEMTECH_RADIO)
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    pinMode(BOARD_LORA_RST, OUTPUT);
    digitalWrite(BOARD_LORA_RST, HIGH);
#endif
    pinMode(BOARD_EPD_CS, OUTPUT);
    digitalWrite(BOARD_EPD_CS, HIGH);

    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    ui_input_ip5356_key_init();
#if USE_IP5356_CHARGER
    (void)w20PowerBatteryInit();
    peri_init_st[E_PERI_IP5356] = w20_ip5356_is_present();
#endif

    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_INT);
    peri_init_st[E_PERI_INK_SCREEN] = ink_screen_init_hsipi();
    peri_init_st[E_PERI_TOUCH] = touch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_TOUCH_SDA, BOARD_TOUCH_SCL);

    lvgl_init_shell();
    ui_deckpro_entry();
    peri_lighting_led_key_init();
    disp_full_refr();
    w20_deckpro_present_menu_on_epd();

    peri_init_st[E_PERI_LORA] = false;
    peri_init_st[E_PERI_GPS] = false;

    /* 默认 MeshCore Semtech 持有 LoRa；进 LoRa 子菜单 request_deck，退出后 request_mesh。 */
    w20_lora_arbiter_request_mesh();
    w20_gps_arbiter_request_mesh();

    s_menu_page_index = 0;
    s_epd_reuse_ready = true;
    w20_shell_set_mesh_touch_input(false);
    /* Boot 超时约 5s 会入队 STOP_BOOT_SCREEN；须在其后再 prewarm 才能排空队列。 */
    s_mesh_prewarm_due_ms = millis() + 6500;
}

void w20_deckpro_shell_loop(void)
{
    if (!w20_deck_sleep_active()) {
        ui_deckpro_poll_gesture();
        ui_deckpro_poll_power_ui();
        ui_input_ip5356_key_poll();
        w20_battery_dual_soc_log_poll();
        ui_deckpro_poll_low_batt_shutdown();
        if (!w20_shell_mesh_screen_allowed()) {
            ui_deckpro_poll_boot_shutdown();
        } else if (screen != nullptr) {
            /* overlay 切项合并刷墨：在 shell 主循环兑现，避免卡在触摸回调里连刷。 */
            screen->w20PollOverlayNavEpdFlush();
        }
    }
    if (!w20_deck_sleep_active() && s_menu_page_index == 0 && !s_mesh_ui_prewarmed && s_mesh_prewarm_due_ms != 0 &&
        millis() >= s_mesh_prewarm_due_ms && screen != nullptr) {
        w20_shell_prewarm_mesh_ui_impl();
    }
    if (s_mesh_present_pending && !w20_deck_sleep_active()) {
        w20_shell_process_mesh_enter_pending();
    }
    w20_lora_arbiter_tick();
    w20_gps_arbiter_tick();
    w20_wifi_ota_loop();
#if !MESHTASTIC_EXCLUDE_WEBSERVER
    w20_tryCompleteDeferredWebServer();
#endif
    if (w20_deck_sleep_active()) {
        w20_deck_sleep_poll_boot();
    } else if (!w20_shell_mesh_screen_allowed()) {
        lv_task_handler();
        w20_deckpro_epd_flush_if_pending();
        w20_wifi_ota_loop();
#ifdef MESHCORE_DECK_SHELL
    } else {
        /* Mesh 页：跑 MeshCore companion UI（共享 shell EPD） */
        w20_meshcore_ui_loop();
#endif
    }
    peri_lighting_led_key_poll();
    delay((w20_wifi_ota_is_busy() || w20_wifi_ota_is_running()) ? 1 : 5);
}

#else

#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"

bool w20_shell_mesh_screen_allowed(void)
{
    return true;
}
void w20_shell_on_menu_page(int) {}
void w20_deckpro_shell_init(void) {}
void w20_deckpro_shell_loop(void) {}
void w20_deckpro_early_splash(void) {}
void w20_deckpro_sleep_prompt_flush(void) {}

#endif
