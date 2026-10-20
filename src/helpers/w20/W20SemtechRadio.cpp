#if defined(W20_USE_SEMTECH_RADIO)

#include "W20SemtechRadio.h"

#include <math.h>
#include <driver/gpio.h>

extern "C" {
#include "apps_common.h"
#include "lr20xx_hal.h"
#include "lr20xx_radio_common.h"
#include "lr20xx_radio_fifo.h"
#include "lr20xx_radio_lora.h"
#include "lr20xx_system.h"
#include "smtc_shield_lr20xx_interface.h"
}

#ifndef LR2021_RX_BOOST_LEVEL
#define LR2021_RX_BOOST_LEVEL 7
#endif

#ifndef P_LORA_EN
#define P_LORA_EN 39
#endif

W20SemtechRadio *W20SemtechRadio::s_inst = nullptr;

#define IRQ_RX (1u << 0)
#define IRQ_TX (1u << 1)
#define IRQ_ERR (1u << 2)

/* Only completion / error IRQs. Preamble/sync fire too often under RF and must
 * never drive SPI from the GPIO ISR (INT_WDT when peer TX / noise). */
#define IRQ_MASK                                                                                                       \
  (LR20XX_SYSTEM_IRQ_RX_DONE | LR20XX_SYSTEM_IRQ_TX_DONE | LR20XX_SYSTEM_IRQ_TIMEOUT | LR20XX_SYSTEM_IRQ_CRC_ERROR |   \
   LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR)

void IRAM_ATTR W20SemtechRadio::onDioIsr() {
  if (!s_inst)
    return;
  s_inst->_dio_pending = true;
}

static lr20xx_radio_lora_sf_t map_sf(uint8_t sf) {
  if (sf < 5)
    sf = 5;
  if (sf > 12)
    sf = 12;
  return (lr20xx_radio_lora_sf_t)sf;
}

static lr20xx_radio_lora_bw_t map_bw(float khz) {
  if (khz < 20.0f)
    return LR20XX_RADIO_LORA_BW_15;
  if (khz < 41.0f)
    return LR20XX_RADIO_LORA_BW_31;
  if (khz < 83.0f)
    return LR20XX_RADIO_LORA_BW_62;
  if (khz < 187.0f)
    return LR20XX_RADIO_LORA_BW_125;
  if (khz < 375.0f)
    return LR20XX_RADIO_LORA_BW_250;
  if (khz < 650.0f)
    return LR20XX_RADIO_LORA_BW_500;
  if (khz < 900.0f)
    return LR20XX_RADIO_LORA_BW_812;
  return LR20XX_RADIO_LORA_BW_1000;
}

static lr20xx_radio_lora_cr_t map_cr(uint8_t cr) {
  switch (cr) {
  case 6:
    return LR20XX_RADIO_LORA_CR_4_6;
  case 7:
    return LR20XX_RADIO_LORA_CR_4_7;
  case 8:
    return LR20XX_RADIO_LORA_CR_4_8;
  default:
    return LR20XX_RADIO_LORA_CR_4_5;
  }
}

bool W20SemtechRadio::hwInit() {
  gpio_hold_dis((gpio_num_t)P_LORA_EN);
  pinMode(P_LORA_EN, OUTPUT);
  digitalWrite(P_LORA_EN, HIGH);
  delay(50);

  Serial.printf("W20Semtech: EN=%d BUSY=%d CPU=%uMHz\n", digitalRead(P_LORA_EN),
                digitalRead(P_LORA_BUSY), (unsigned)getCpuFrequencyMhz());

  if (digitalRead(P_LORA_EN) != HIGH) {
    Serial.println("W20Semtech: EN low");
    return false;
  }

  _ctx = apps_common_lr20xx_get_radio_context();
  if (!_ctx) {
    Serial.println("W20Semtech: no radio context");
    return false;
  }

  if (lr20xx_hal_init() != LR20XX_HAL_STATUS_OK) {
    Serial.println("W20Semtech: HAL init failed");
    _ctx = nullptr;
    return false;
  }

  apps_common_lr20xx_system_init((const lr20xx_hal_context_t *)_ctx);

  lr20xx_system_version_t ver = {};
  if (lr20xx_system_get_version(_ctx, &ver) != LR20XX_STATUS_OK) {
    Serial.println("W20Semtech: get_version failed");
    return false;
  }
  Serial.printf("W20Semtech: LR2021 FW %u.%u\n", (unsigned)ver.major, (unsigned)ver.minor);
  if (ver.major == 0xFF || ver.minor == 0xFF || (ver.major == 0 && ver.minor == 0)) {
    Serial.println("W20Semtech: bad version (SPI dead)");
    lr20xx_hal_reset(_ctx);
    apps_common_lr20xx_system_init((const lr20xx_hal_context_t *)_ctx);
    ver = {};
    lr20xx_system_get_version(_ctx, &ver);
    Serial.printf("W20Semtech: retry FW %u.%u\n", (unsigned)ver.major, (unsigned)ver.minor);
    if (ver.major == 0xFF || ver.minor == 0xFF || (ver.major == 0 && ver.minor == 0)) {
      _ctx = nullptr;
      return false;
    }
  }

  const lr20xx_system_dio_t *irq_dio = smtc_shield_lr20xx_get_irq_dio();
  const apps_common_lr20xx_irq_cfg_t irq_cfg[] = {
      {.irq_mask = IRQ_MASK, .dio = *irq_dio},
  };
  apps_common_lr20xx_set_dio_irq_params(_ctx, irq_cfg, 1);

  s_inst = this;
  lr20xx_set_mesh_dio_irq_cb(onDioIsr);
  applyModem();
  _ready = true;
  return true;
}

void W20SemtechRadio::hwStandby() {
  lr20xx_set_mesh_dio_irq_cb(nullptr);
  if (_ctx)
    lr20xx_system_set_standby_mode(_ctx, LR20XX_SYSTEM_STANDBY_MODE_RC);
  _in_rx = false;
  _irq_flags = 0;
  _dio_pending = false;
}

void W20SemtechRadio::applyModem() {
  if (!_ctx)
    return;
  const uint32_t freqHz = (uint32_t)lroundf(_freq * 1e6f);
  int pwrHalf = (int)_tx_dbm * 2;
  if (pwrHalf < 0)
    pwrHalf = 0;
  if (pwrHalf > 44)
    pwrHalf = 44;

  my_apps_common_lr20xx_radio_init_first(_ctx, LR20XX_RADIO_COMMON_PKT_TYPE_LORA, freqHz, (uint8_t)pwrHalf);

  lr20xx_radio_lora_mod_params_t mod = {
      .sf = map_sf(_sf),
      .bw = map_bw(_bw),
      .cr = map_cr(_cr),
      .ppm = LR20XX_RADIO_LORA_NO_PPM,
  };
  mod.ppm = (lr20xx_radio_lora_ppm_t)apps_common_compute_lora_ppm(mod.sf, mod.bw);
  lr20xx_radio_lora_set_modulation_params(_ctx, &mod);

  lr20xx_radio_lora_pkt_params_t pkt = {
      .preamble_len_in_symb = _preamble,
      .pkt_mode = LR20XX_RADIO_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = 0,
      .crc = LR20XX_RADIO_LORA_CRC_ENABLED,
      .iq = LR20XX_RADIO_LORA_IQ_STANDARD,
  };
  lr20xx_radio_lora_set_packet_params(_ctx, &pkt);
  lr20xx_radio_lora_set_syncword(_ctx, 0x12);

  if (_rx_boosted) {
    // boost via path already set in radio_init_first; keep flag for prefs
  }
}

void W20SemtechRadio::startRecvHw() {
  if (!_ctx)
    return;
  lr20xx_radio_lora_pkt_params_t pkt = {
      .preamble_len_in_symb = _preamble,
      .pkt_mode = LR20XX_RADIO_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = 0,
      .crc = LR20XX_RADIO_LORA_CRC_ENABLED,
      .iq = LR20XX_RADIO_LORA_IQ_STANDARD,
  };
  lr20xx_radio_lora_set_packet_params(_ctx, &pkt);
  lr20xx_radio_common_set_rx(_ctx, 0);
  _in_rx = true;
  lr20xx_set_mesh_dio_irq_cb(onDioIsr);
}

bool W20SemtechRadio::std_init() {
  if (_ready)
    return true;
  if (!hwInit())
    return false;
  startRecvHw();
  Serial.println("W20Semtech: radio init OK");
  return true;
}

void W20SemtechRadio::powerOff() {
  hwStandby();
  if (_ctx) {
    lr20xx_system_sleep_cfg_t cfg = {.is_clk_32k_enabled = false, .is_ram_retention_enabled = true};
    lr20xx_system_set_sleep_mode(_ctx, &cfg, 0);
  }
  _ready = false;
}

void W20SemtechRadio::shutdownForDeck() {
  hwStandby();
  /* Keep HAL SPI device; peri_lora reuses same SPI2_HOST. Mark mesh idle. */
  _ready = false;
  Serial.println("W20Semtech: shutdown for Deck");
}

bool W20SemtechRadio::resumeAfterDeck() {
  /* peri_lora may have left driver "released"; re-run system_init + modem */
  if (!_ctx)
    _ctx = apps_common_lr20xx_get_radio_context();
  if (!_ctx)
    return false;
  if (lr20xx_hal_init() != LR20XX_HAL_STATUS_OK)
    return false;
  apps_common_lr20xx_system_init((const lr20xx_hal_context_t *)_ctx);
  lr20xx_system_version_t ver = {};
  lr20xx_system_get_version(_ctx, &ver);
  if (ver.major == 0xFF || (ver.major == 0 && ver.minor == 0)) {
    Serial.printf("W20Semtech: resume bad FW %u.%u\n", (unsigned)ver.major, (unsigned)ver.minor);
    return false;
  }
  const lr20xx_system_dio_t *irq_dio = smtc_shield_lr20xx_get_irq_dio();
  const apps_common_lr20xx_irq_cfg_t irq_cfg[] = {
      {.irq_mask = IRQ_MASK, .dio = *irq_dio},
  };
  apps_common_lr20xx_set_dio_irq_params(_ctx, irq_cfg, 1);
  s_inst = this;
  _dio_pending = false;
  _irq_flags = 0;
  lr20xx_set_mesh_dio_irq_cb(onDioIsr);
  applyModem();
  _ready = true;
  startRecvHw();
  Serial.println("W20Semtech: resumed after Deck");
  return true;
}

void W20SemtechRadio::begin() {
  _nf_samples = 0;
  _nf_sum = 0;
  _noise_floor = -100;
  if (_ready)
    startRecvHw();
}

void W20SemtechRadio::processIrq() {
  if (!_ready || !_ctx)
    return;
  /* Rising-edge ISR only sets a flag; DIO stays high until SPI clear — also poll. */
  if (!_dio_pending && gpio_get_level((gpio_num_t)LORA_IRQ_PIN) == 0)
    return;
  _dio_pending = false;
  lr20xx_system_irq_mask_t irq = LR20XX_SYSTEM_IRQ_NONE;
  if (lr20xx_system_get_and_clear_irq_status(_ctx, &irq) != LR20XX_STATUS_OK)
    return;
  if (irq & LR20XX_SYSTEM_IRQ_RX_DONE)
    _irq_flags |= IRQ_RX;
  if (irq & LR20XX_SYSTEM_IRQ_TX_DONE)
    _irq_flags |= IRQ_TX;
  if (irq & (LR20XX_SYSTEM_IRQ_CRC_ERROR | LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR | LR20XX_SYSTEM_IRQ_TIMEOUT))
    _irq_flags |= IRQ_ERR;
}

int W20SemtechRadio::recvRaw(uint8_t *bytes, int sz) {
  if (!_ready || !_ctx)
    return 0;

  processIrq();

  if (_irq_flags & IRQ_ERR) {
    _irq_flags &= ~(IRQ_ERR | IRQ_RX);
    n_recv_errors++;
    startRecvHw();
    return 0;
  }

  if (!(_irq_flags & IRQ_RX)) {
    if (!_in_rx)
      startRecvHw();
    return 0;
  }
  _irq_flags &= ~IRQ_RX;

  uint16_t length = 0;
  if (lr20xx_radio_common_get_rx_packet_length(_ctx, &length) != LR20XX_STATUS_OK) {
    n_recv_errors++;
    startRecvHw();
    return 0;
  }
  if (length == 0 || length > (uint16_t)sz) {
    n_recv_errors++;
    startRecvHw();
    return 0;
  }
  if (lr20xx_radio_fifo_read_rx(_ctx, bytes, length) != LR20XX_STATUS_OK) {
    n_recv_errors++;
    startRecvHw();
    return 0;
  }
  lr20xx_radio_lora_packet_status_t st = {};
  if (lr20xx_radio_lora_get_packet_status(_ctx, &st) == LR20XX_STATUS_OK) {
    _last_rssi = (float)st.rssi_pkt_in_dbm;
    _last_snr = st.snr_pkt_raw / 4.0f;
  }
  n_recv++;
  startRecvHw();
  return (int)length;
}

uint32_t W20SemtechRadio::getEstAirtimeFor(int len_bytes) {
  if (!_ctx)
    return 100;
  lr20xx_radio_lora_mod_params_t mod = {
      .sf = map_sf(_sf),
      .bw = map_bw(_bw),
      .cr = map_cr(_cr),
      .ppm = LR20XX_RADIO_LORA_NO_PPM,
  };
  mod.ppm = (lr20xx_radio_lora_ppm_t)apps_common_compute_lora_ppm(mod.sf, mod.bw);
  lr20xx_radio_lora_pkt_params_t pkt = {
      .preamble_len_in_symb = _preamble,
      .pkt_mode = LR20XX_RADIO_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = (uint8_t)((len_bytes > 255) ? 255 : len_bytes),
      .crc = LR20XX_RADIO_LORA_CRC_ENABLED,
      .iq = LR20XX_RADIO_LORA_IQ_STANDARD,
  };
  uint32_t ms = lr20xx_radio_lora_get_time_on_air_in_ms(&pkt, &mod);
  return ms ? ms : 1;
}

bool W20SemtechRadio::startSendRaw(const uint8_t *bytes, int len) {
  if (!_ready || !_ctx || len <= 0 || len > 255)
    return false;
  if (_board)
    _board->onBeforeTransmit();

  hwStandby();
  lr20xx_radio_lora_pkt_params_t pkt = {
      .preamble_len_in_symb = _preamble,
      .pkt_mode = LR20XX_RADIO_LORA_PKT_EXPLICIT,
      .pld_len_in_bytes = (uint8_t)len,
      .crc = LR20XX_RADIO_LORA_CRC_ENABLED,
      .iq = LR20XX_RADIO_LORA_IQ_STANDARD,
  };
  lr20xx_radio_lora_set_packet_params(_ctx, &pkt);
  lr20xx_status_t st = lr20xx_radio_fifo_write_tx(_ctx, (uint8_t *)bytes, (uint16_t)len);
  if (st == LR20XX_STATUS_OK)
    st = lr20xx_radio_common_set_tx(_ctx, 0);
  if (st != LR20XX_STATUS_OK) {
    if (_board)
      _board->onAfterTransmit();
    startRecvHw();
    return false;
  }
  _irq_flags &= ~(IRQ_TX | IRQ_RX | IRQ_ERR);
  lr20xx_set_mesh_dio_irq_cb(onDioIsr);
  _in_rx = false;
  return true;
}

bool W20SemtechRadio::isSendComplete() {
  processIrq();
  if (_irq_flags & IRQ_TX) {
    _irq_flags &= ~IRQ_TX;
    n_sent++;
    return true;
  }
  return false;
}

void W20SemtechRadio::onSendFinished() {
  if (_board)
    _board->onAfterTransmit();
  startRecvHw();
}

bool W20SemtechRadio::isReceiving() {
  if (!_ready)
    return false;
  processIrq();
  if (_irq_flags & (IRQ_RX | IRQ_ERR))
    return true;
  // preamble / header while in RX — approximate via instant RSSI vs noise
  if (_in_rx && _nf_threshold != 0 && getCurrentRSSI() > _noise_floor + _nf_threshold)
    return true;
  return false;
}

void W20SemtechRadio::loop() {
  if (!_ready)
    return;
  processIrq();
  if (!_in_rx)
    return;
  if (_nf_samples < 64 && !(_irq_flags & IRQ_RX)) {
    int rssi = (int)getCurrentRSSI();
    if (rssi < _noise_floor + 14) {
      _nf_samples++;
      _nf_sum += rssi;
    }
  } else if (_nf_samples >= 64 && _nf_sum != 0) {
    _noise_floor = _nf_sum / 64;
    if (_noise_floor < -120)
      _noise_floor = -120;
    _nf_sum = 0;
  }
}

float W20SemtechRadio::packetScore(float snr, int packet_len) {
  static const float thr[] = {-7.5f, -10, -12.5f, -15, -17.5f, -20};
  int sf = _sf;
  if (sf < 7)
    return 0;
  if (sf > 12)
    sf = 12;
  if (snr < thr[sf - 7])
    return 0;
  float ok = (snr - thr[sf - 7]) / 10.0f;
  float pen = 1.0f - (packet_len / 256.0f);
  float s = ok * pen;
  if (s < 0)
    s = 0;
  if (s > 1)
    s = 1;
  return s;
}

void W20SemtechRadio::triggerNoiseFloorCalibrate(int threshold) {
  _nf_threshold = threshold;
  _nf_samples = 0;
  _nf_sum = 0;
}

void W20SemtechRadio::resetAGC() {
  if (!_ready)
    return;
  hwStandby();
  startRecvHw();
  _noise_floor = 0;
  _nf_samples = 0;
  _nf_sum = 0;
}

void W20SemtechRadio::setParams(float freq, float bw, uint8_t sf, uint8_t cr) {
  _freq = freq;
  _bw = bw;
  _sf = sf;
  _cr = cr;
  _preamble = (sf <= 8) ? 32 : 16;
  if (_ready) {
    hwStandby();
    applyModem();
    startRecvHw();
  }
}

void W20SemtechRadio::setTxPower(int8_t dbm) {
  _tx_dbm = dbm;
  if (_ready) {
    hwStandby();
    applyModem();
    startRecvHw();
  }
}

bool W20SemtechRadio::setRxBoostedGainMode(bool en) {
  _rx_boosted = en;
  if (_ready) {
    hwStandby();
    applyModem();
    startRecvHw();
  }
  return true;
}

float W20SemtechRadio::getCurrentRSSI() {
  if (!_ctx)
    return -120;
  int16_t rssi = -120;
  apps_common_lr20xx_radio_get_rssi_inst(_ctx, &rssi);
  return (float)rssi;
}

uint32_t W20SemtechRadio::getRngSeed() {
  if (!_ctx)
    return esp_random();
  uint32_t rnd = 0;
  if (lr20xx_system_get_random_number(_ctx, LR20XX_SYSTEM_RANDOM_ENTROPY_SOURCE_ADC, &rnd) != LR20XX_STATUS_OK)
    return esp_random();
  return rnd ^ esp_random();
}

#endif
