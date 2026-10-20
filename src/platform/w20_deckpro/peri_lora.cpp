#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "peripheral.h"
#include "utilities.h"
#include "w20_dbg_log.h"

#if defined(USE_LR2021)

extern "C" {
#include "apps_common.h"
#include "apps_configuration.h"
#include "lr20xx_hal.h"
#include "lr20xx_irq_routines.h"
#include "lr20xx_radio_common.h"
#include "lr20xx_radio_fifo.h"
#include "lr20xx_radio_lora.h"
#include "lr20xx_system.h"
#include "smtc_shield_lr20xx_interface.h"

extern int16_t g_rfRssi;
extern float g_rfSnr;
}

extern "C" {
int16_t g_rfRssi = 0;
float g_rfSnr = 0.0f;
}

#define LORA_SUBG_PWR_HDBM 44
#define LORA_SUBG_FREQ_HZ 915230000U

#define IRQ_MASK                                                                                                       \
    (LR20XX_SYSTEM_IRQ_RX_DONE | LR20XX_SYSTEM_IRQ_TX_DONE | LR20XX_SYSTEM_IRQ_TIMEOUT | LR20XX_SYSTEM_IRQ_CRC_ERROR)

static lr20xx_hal_context_t *s_context = nullptr;
static int lora_mode = LORA_DEFAULT_MODE;
static String lora_recv_data;
static size_t lora_recv_len = 0;
static bool lora_recv_success = false;
static int lora_recv_rssi = 0;
static float lora_recv_snr = 0.0f;
static char lora_last_sent_buf[64] = {0};
static uint8_t s_rfTxBuffer[64] = {0};
static uint8_t s_rfRxBuffer[255] = {0};
static uint32_t s_p2pTxMsgIdx = 0;
static uint32_t lora_next_tx_ms = 0;
static bool s_prev_cw = false;
static bool s_tx_done = false;
static bool s_rx_done = false;
static bool s_lora_driver_ready = false;
static uint32_t s_tx_done_count = 0;
static uint32_t s_tx_ui_count = 0;

static lr20xx_radio_lora_mod_params_t s_lora_mod = {
    .sf = LORA_SPREADING_FACTOR,
    .bw = LORA_BANDWIDTH,
    .cr = LORA_CODING_RATE,
    .ppm = LR20XX_RADIO_LORA_NO_PPM,
};

static lr20xx_radio_lora_pkt_params_t s_lora_pkt = {
    .preamble_len_in_symb = LORA_PREAMBLE_LENGTH,
    .pkt_mode = LORA_PKT_LEN_MODE,
    .pld_len_in_bytes = 0,
    .crc = LORA_CRC,
    .iq = LORA_IQ,
};

static bool lora_mode_is_cw(int mode)
{
    return mode == LORA_MODE_CW;
}

static void lora_log_radio_status(const char *tag);
static void lora_radio_hw_prepare(void);
static void lora_xosc_probe_once(const char *tag);

static void lora_setup_irq_params(void)
{
    if (!s_context) {
        return;
    }
    const lr20xx_system_dio_t *irq_dio = smtc_shield_lr20xx_get_irq_dio();
    const apps_common_lr20xx_irq_cfg_t irq_cfg[] = {
        {.irq_mask = IRQ_MASK, .dio = *irq_dio},
    };
    apps_common_lr20xx_set_dio_irq_params(s_context, irq_cfg, sizeof(irq_cfg) / sizeof(irq_cfg[0]));
}

static void lora_apply_lora_params_rx_listen(void)
{
    s_lora_mod.ppm = (lr20xx_radio_lora_ppm_t)apps_common_compute_lora_ppm(s_lora_mod.sf, s_lora_mod.bw);
    lr20xx_radio_lora_set_modulation_params(s_context, &s_lora_mod);
    s_lora_pkt.pld_len_in_bytes = 0;
    lr20xx_radio_lora_set_packet_params(s_context, &s_lora_pkt);
    lr20xx_radio_lora_set_syncword(s_context, LORA_SYNCWORD);
}

/** GPIO 边沿漏检时轮询芯片 IRQ（对齐 w20_TXRX lr20xx_demo） */
static void lora_irq_poll_fallback(void)
{
    if (!s_context) {
        return;
    }

    lr20xx_system_stat1_t st1 = {};
    lr20xx_system_stat2_t st2 = {};
    lr20xx_system_irq_mask_t pending = 0;

    if (lr20xx_system_get_status(s_context, &st1, &st2, &pending) != LR20XX_STATUS_OK) {
        return;
    }

    if (!st1.is_interrupt_active && (pending & IRQ_MASK) == 0) {
        return;
    }

    lr20xx_system_irq_mask_t irq_regs = 0;
    if (lr20xx_system_get_and_clear_irq_status(s_context, &irq_regs) != LR20XX_STATUS_OK) {
        return;
    }
    irq_regs &= IRQ_MASK;
    if (irq_regs == 0) {
        return;
    }

    if (irq_regs & LR20XX_SYSTEM_IRQ_TIMEOUT) {
        lr20xx_timeout_irq_handler();
    }
    if (irq_regs & LR20XX_SYSTEM_IRQ_RX_DONE) {
        if (irq_regs & LR20XX_SYSTEM_IRQ_CRC_ERROR) {
            lr20xx_crc_error_irq_handler();
        } else {
            lr20xx_rx_done_irq_handler();
        }
    } else if (irq_regs & LR20XX_SYSTEM_IRQ_CRC_ERROR) {
        lr20xx_crc_error_irq_handler();
    }
    if (irq_regs & LR20XX_SYSTEM_IRQ_TX_DONE) {
        lr20xx_tx_done_irq_handler();
    }
}

static void lora_process_irqs(void)
{
    apps_common_lr20xx_irq_process(s_context, IRQ_MASK, false);
    lora_irq_poll_fallback();
}

static const char *build_hello_payload(void)
{
    int msg_len = snprintf((char *)s_rfTxBuffer, sizeof(s_rfTxBuffer), "hello %lu", (unsigned long)s_p2pTxMsgIdx++);
    if (msg_len < 0) {
        return NULL;
    }
    if (msg_len >= (int)sizeof(s_rfTxBuffer)) {
        msg_len = (int)sizeof(s_rfTxBuffer) - 1;
    }
    strncpy(lora_last_sent_buf, (const char *)s_rfTxBuffer, sizeof(lora_last_sent_buf) - 1);
    lora_last_sent_buf[sizeof(lora_last_sent_buf) - 1] = '\0';
    return (const char *)s_rfTxBuffer;
}

static void lora_send_one_packet(void)
{
    const char *payload = build_hello_payload();
    if (payload == NULL) {
        return;
    }
    uint8_t tx_len = (uint8_t)strlen(payload);
    /* 对齐可工作 Deck 工程：STBY_RC -> set_tx，不做 XOSC 门禁 abort */
    lr20xx_system_set_standby_mode(s_context, LR20XX_SYSTEM_STANDBY_MODE_RC);
    uint16_t errors = 0;
    (void)lr20xx_system_get_errors(s_context, &errors);
    if (errors != 0) {
        (void)lr20xx_system_clear_errors(s_context);
    }
    (void)lr20xx_system_clear_irq_status(s_context, LR20XX_SYSTEM_IRQ_ALL_MASK);
    s_lora_pkt.pld_len_in_bytes = tx_len;
    lr20xx_radio_lora_set_packet_params(s_context, &s_lora_pkt);
    lr20xx_radio_fifo_write_tx(s_context, s_rfTxBuffer, tx_len);
    lr20xx_status_t st = lr20xx_radio_common_set_tx(s_context, 0);
    lora_next_tx_ms = millis() + LORA_TX_INTERVAL_MS;
    if (st != LR20XX_STATUS_OK) {
        Serial.printf("[LoRa] set_tx FAIL st=%d len=%u\n", (int)st, (unsigned)tx_len);
    } else {
        W20_LORA_LOG("[LoRa] TX cmd OK '%s'\n", lora_last_sent_buf);
    }
    delay(2);
    lora_log_radio_status("TX");
}

static void lora_log_radio_status(const char *tag)
{
    if (!s_context) {
        return;
    }
    lr20xx_system_stat1_t st1 = {};
    lr20xx_system_stat2_t st2 = {};
    lr20xx_system_irq_mask_t pending = 0;
    uint16_t errors = 0;
    if (lr20xx_system_get_status(s_context, &st1, &st2, &pending) != LR20XX_STATUS_OK) {
        Serial.printf("[LoRa] %s: get_status FAIL\n", tag);
        return;
    }
    (void)lr20xx_system_get_errors(s_context, &errors);
    /* mode: 1=STBY_RC 2=STBY_XOSC 3=FS 4=RX 5=TX - 量功率需要 mode=5 */
    if (errors & LR20XX_SYSTEM_ERRORS_HF_XOSC_START_MASK) {
        Serial.printf("[LoRa] %s: mode=%u irq=0x%lX err=0x%04x HF_XOSC_FAIL cmd=%u\n", tag, (unsigned)st2.chip_mode,
                      (unsigned long)pending, (unsigned)errors, (unsigned)st1.command_status);
    } else {
        W20_LORA_LOG("[LoRa] %s: mode=%u irq=0x%lX err=0x%04x%s cmd=%u\n", tag, (unsigned)st2.chip_mode,
                        (unsigned long)pending, (unsigned)errors,
                        (st2.chip_mode == LR20XX_SYSTEM_CHIP_MODE_TX) ? " (TX_ON)" : "",
                        (unsigned)st1.command_status);
    }
}

/** 启动后探测一次时钟；失败只打日志，不阻断 TX（参考工程也不阻断） */
static void lora_xosc_probe_once(const char *tag)
{
    if (!s_context) {
        return;
    }
    lr20xx_status_t st = lr20xx_system_set_standby_mode(s_context, LR20XX_SYSTEM_STANDBY_MODE_XOSC);
    delay(10);
    uint16_t errors = 0;
    (void)lr20xx_system_get_errors(s_context, &errors);
    const bool fail = (errors & LR20XX_SYSTEM_ERRORS_HF_XOSC_START_MASK) != 0;
    if (fail) {
        Serial.printf("[LoRa] %s: probe_XOSC st=%d err=0x%04x HF_XOSC_FAIL\n", tag, (int)st, (unsigned)errors);
    } else {
        W20_LORA_LOG("[LoRa] %s: probe_XOSC st=%d err=0x%04x OK\n", tag, (int)st, (unsigned)errors);
    }
    if (errors) {
        (void)lr20xx_system_clear_errors(s_context);
    }
    (void)lr20xx_system_clear_irq_status(s_context, LR20XX_SYSTEM_IRQ_ALL_MASK);
    (void)lr20xx_system_set_standby_mode(s_context, LR20XX_SYSTEM_STANDBY_MODE_RC);
}

static void lora_radio_hw_prepare(void)
{
    my_apps_common_lr20xx_radio_init_first((const lr20xx_hal_context_t *)s_context, LR20XX_RADIO_COMMON_PKT_TYPE_LORA,
                                           LORA_SUBG_FREQ_HZ, LORA_SUBG_PWR_HDBM);
    /* 对齐 demo：保留 init_first 的 FALLBACK_STDBY_XOSC，不再改成 STBY_RC */
    lora_setup_irq_params();
}

static void lora_apply_mode_internal(int mode, bool first_init)
{
    const bool is_cw = lora_mode_is_cw(mode);
    (void)first_init;

    s_tx_done = false;
    s_rx_done = false;

    lr20xx_system_set_standby_mode(s_context, LR20XX_SYSTEM_STANDBY_MODE_RC);

    /* 每次切模式都重配频点/PA（Mesh↔Deck 切换后尤其需要） */
    lora_radio_hw_prepare();

    if (is_cw) {
        lr20xx_status_t st =
            lr20xx_radio_common_set_tx_test_mode(s_context, LR20XX_RADIO_COMMON_TX_TEST_MODE_CONTINUOUS_WAVE);
        Serial.printf("[LoRa] CW start @%lu Hz %u hdbm st=%d\n", (unsigned long)LORA_SUBG_FREQ_HZ,
                      (unsigned)LORA_SUBG_PWR_HDBM, (int)st);
        delay(5);
        lora_log_radio_status("CW");
    } else {
        lr20xx_radio_common_set_tx_test_mode(s_context, LR20XX_RADIO_COMMON_TX_TEST_MODE_NORMAL);
        lora_apply_lora_params_rx_listen();
        if (mode == LORA_MODE_RECV) {
            lr20xx_radio_common_set_rx(s_context, 0);
            delay(2);
            lora_log_radio_status("RX");
        }
    }

    lora_mode = mode;
    s_prev_cw = is_cw;
}

extern "C" void lr20xx_tx_done_irq_handler(void)
{
    if (lora_mode_is_cw(lora_mode)) {
        return;
    }
    s_tx_done = true;
    s_tx_done_count++;
}

extern "C" void lr20xx_rx_done_irq_handler(void)
{
    uint16_t size = 0;

    if (lora_mode_is_cw(lora_mode) || !s_context) {
        return;
    }

    memset(s_rfRxBuffer, 0, sizeof(s_rfRxBuffer));
    apps_common_lr20xx_receive(s_context, s_rfRxBuffer, sizeof(s_rfRxBuffer), &size);
    if (size > sizeof(s_rfRxBuffer)) {
        size = sizeof(s_rfRxBuffer);
    }
    lora_recv_len = (size_t)size;
    lora_recv_success = true;
    lora_recv_data = "";
    if (lora_recv_len > 0) {
        lora_recv_data.reserve(lora_recv_len);
        for (size_t i = 0; i < lora_recv_len; ++i) {
            char c = (char)s_rfRxBuffer[i];
            lora_recv_data += isprint((unsigned char)c) ? c : '.';
        }
    }
    lora_recv_rssi = (int)g_rfRssi;
    lora_recv_snr = g_rfSnr;
    W20_LORA_LOG("[LoRa] RX ok len=%u rssi=%d snr=%.1f text='%.*s'\n", (unsigned)lora_recv_len, lora_recv_rssi,
                  (double)lora_recv_snr, (int)lora_recv_len, lora_recv_data.c_str());

    lr20xx_radio_common_set_rx(s_context, 0);
    s_rx_done = true;
}

extern "C" void lr20xx_timeout_irq_handler(void) {}

extern "C" void lr20xx_crc_error_irq_handler(void)
{
    if (lora_mode_is_cw(lora_mode) || !s_context) {
        return;
    }
    Serial.printf("[LoRa] RX CRC error (heard energy, bad CRC)\n");
    lr20xx_radio_fifo_clear_rx(s_context);
    lr20xx_radio_common_set_rx(s_context, 0);
}

bool lora_init(void)
{
    lr20xx_set_mesh_dio_irq_cb(nullptr);

    if (s_lora_driver_ready && s_context) {
        return true;
    }

#if (BOARD_LORA_EN >= 0)
    if (digitalRead(BOARD_LORA_EN) != HIGH) {
        Serial.println(F("[LoRa] init skipped: module power OFF (EN low)"));
        return false;
    }
#endif

    s_context = (lr20xx_hal_context_t *)apps_common_lr20xx_get_radio_context();
    if (!s_context) {
        Serial.println(F("[LoRa] no radio context"));
        return false;
    }

    if (lr20xx_hal_init() != LR20XX_HAL_STATUS_OK) {
        Serial.println(F("[LoRa] HAL init failed"));
        return false;
    }

    apps_common_lr20xx_system_init((const lr20xx_hal_context_t *)s_context);

#ifndef W20_LORA_HAS_TCXO
#define W20_LORA_HAS_TCXO 1
#endif
    W20_LORA_LOG("[LoRa] crystal cfg: W20_LORA_HAS_TCXO=%d (%s)\n", (int)W20_LORA_HAS_TCXO,
                 W20_LORA_HAS_TCXO ? "active TCXO" : "passive XTAL");

    /* 探测 TCXO/XOSC 一次；失败只打日志，不阻断后续 TX */
    lora_xosc_probe_once("boot");

    lr20xx_system_version_t version = {0};
    lr20xx_system_get_version(s_context, &version);
    Serial.printf("[LoRa] LR2021 version %d.%d\n", version.major, version.minor);

    /* 255.255 / 0.0 = SPI dead (often CS stolen by gpio_config after mesh handoff). */
    if (version.major == 0xFF || version.minor == 0xFF ||
        (version.major == 0 && version.minor == 0)) {
        Serial.println(F("[LoRa] bad version, hard-reset retry"));
        lr20xx_hal_reset(s_context);
        apps_common_lr20xx_system_init((const lr20xx_hal_context_t *)s_context);
        version = (lr20xx_system_version_t){0};
        lr20xx_system_get_version(s_context, &version);
        Serial.printf("[LoRa] LR2021 version retry %d.%d\n", version.major, version.minor);
        if (version.major == 0xFF || version.minor == 0xFF ||
            (version.major == 0 && version.minor == 0)) {
            Serial.println(F("[LoRa] init failed: SPI/version still invalid"));
            s_context = nullptr;
            s_lora_driver_ready = false;
            return false;
        }
    }

    lora_setup_irq_params();

    lora_last_sent_buf[0] = '\0';
    s_tx_done_count = 0;
    s_tx_ui_count = 0;
    lora_apply_mode_internal(LORA_DEFAULT_MODE, true);
    s_lora_driver_ready = true;
    Serial.println(F("[LoRa] init OK (LR2021)"));
    return true;
}

void lora_set_mode(int mode)
{
    if (!s_lora_driver_ready || !s_context) {
        return;
    }

    lora_apply_mode_internal(mode, false);

    if (mode == LORA_MODE_SEND) {
        s_p2pTxMsgIdx = 0;
        s_tx_done_count = 0;
        s_tx_ui_count = 0;
        lora_send_one_packet();
    }
}

int lora_get_mode(void)
{
    return lora_mode;
}

void lora_receive_loop(void)
{
    if (!s_lora_driver_ready || lora_mode != LORA_MODE_RECV) {
        return;
    }

    lora_process_irqs();

    if (s_rx_done) {
        s_rx_done = false;
    }
}

void lora_transmit(const char *str)
{
    (void)str;
    if (!s_lora_driver_ready || lora_mode != LORA_MODE_SEND) {
        return;
    }

    lora_process_irqs();

    if (s_tx_done) {
        s_tx_done = false;
    }

    if ((int32_t)(millis() - lora_next_tx_ms) >= 0) {
        lora_send_one_packet();
    }
}

const char *lora_get_last_sent(void)
{
    return lora_last_sent_buf;
}

bool lora_take_tx_for_ui(const char **out)
{
    if (s_tx_ui_count >= s_tx_done_count) {
        return false;
    }
    s_tx_ui_count++;
    if (out) {
        *out = lora_last_sent_buf;
    }
    return true;
}

bool lora_get_recv(const char **str, int *rssi, float *snr)
{
    *str = lora_recv_data.c_str();
    *rssi = lora_recv_rssi;
    if (snr) {
        *snr = lora_recv_snr;
    }
    return lora_recv_success;
}

void lora_set_recv_flag(void)
{
    lora_recv_success = false;
}

void lora_sleep(void)
{
    if (s_context) {
        lr20xx_system_set_standby_mode(s_context, LR20XX_SYSTEM_STANDBY_MODE_RC);
    }
}

void lora_release_for_mesh(void)
{
#if defined(ARDUINO_ARCH_ESP32)
    lr20xx_set_mesh_dio_irq_cb(nullptr);
    lora_sleep();
    /* Do not pinMode(CS): SPI HAL owns CS. Only pulse RST for a clean handoff. */
    pinMode(BOARD_LORA_RST, OUTPUT);
    digitalWrite(BOARD_LORA_RST, LOW);
    delay(5);
    digitalWrite(BOARD_LORA_RST, HIGH);
    delay(5);
    s_lora_driver_ready = false;
    s_context = nullptr;
#endif
}

#else /* !USE_LR2021 */

bool lora_init(void) { return false; }
void lora_set_mode(int mode) { (void)mode; }
int lora_get_mode(void) { return LORA_DEFAULT_MODE; }
void lora_receive_loop(void) {}
void lora_transmit(const char *str) { (void)str; }
const char *lora_get_last_sent(void) { return ""; }
bool lora_take_tx_for_ui(const char **out)
{
    if (out)
        *out = NULL;
    return false;
}
bool lora_get_recv(const char **str, int *rssi, float *snr)
{
    if (str)
        *str = "";
    if (rssi)
        *rssi = 0;
    if (snr)
        *snr = 0;
    return false;
}
void lora_set_recv_flag(void) {}
void lora_sleep(void) {}
void lora_release_for_mesh(void) {}

#endif // USE_LR2021
