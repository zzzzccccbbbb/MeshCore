#pragma once

#if defined(W20_USE_SEMTECH_RADIO)

#include <Arduino.h>
#include <Mesh.h>

/**
 * MeshCore radio backend for Meshnology W20 LR2021 using Semtech esp_lora_lr2021
 * (same HAL path as w20_meshtastic). Replaces CustomLR2021 / RadioLib on this board.
 */
class W20SemtechRadio : public mesh::Radio {
  mesh::MainBoard *_board = nullptr;
  void *_ctx = nullptr;
  bool _ready = false;
  bool _in_rx = false;
  bool _rx_boosted = false;

  float _freq =
#ifdef LORA_FREQ
      LORA_FREQ
#else
      869.618f
#endif
      ;
  float _bw =
#ifdef LORA_BW
      LORA_BW
#else
      62.5f
#endif
      ;
  uint8_t _sf =
#ifdef LORA_SF
      LORA_SF
#else
      8
#endif
      ;
  uint8_t _cr =
#ifdef LORA_CR
      LORA_CR
#else
      5
#endif
      ;
  int8_t _tx_dbm =
#ifdef LORA_TX_POWER
      LORA_TX_POWER
#else
      22
#endif
      ;
  uint16_t _preamble = 16;

  volatile uint8_t _irq_flags = 0; // bit0=RX_DONE, bit1=TX_DONE, bit2=ERR
  float _last_rssi = -120;
  float _last_snr = 0;
  int16_t _noise_floor = -100;
  uint16_t _nf_samples = 0;
  int32_t _nf_sum = 0;
  bool _cad_enabled = false;
  int _nf_threshold = 0;

  uint32_t n_recv = 0, n_sent = 0, n_recv_errors = 0;

  static void IRAM_ATTR onDioIsr();
  static W20SemtechRadio *s_inst;

  bool hwInit();
  void hwStandby();
  void applyModem();
  void startRecvHw();
  void processIrq();

public:
  explicit W20SemtechRadio(mesh::MainBoard &board) : _board(&board) {}

  bool std_init();
  void shutdownForDeck();
  bool resumeAfterDeck();
  bool isReady() const { return _ready; }
  void powerOff();

  void begin() override;
  int recvRaw(uint8_t *bytes, int sz) override;
  uint32_t getEstAirtimeFor(int len_bytes) override;
  bool startSendRaw(const uint8_t *bytes, int len) override;
  bool isSendComplete() override;
  void onSendFinished() override;
  bool isInRecvMode() const override { return _in_rx; }
  bool isReceiving() override;
  void loop() override;
  float packetScore(float snr, int packet_len) override;
  float getLastRSSI() const override { return _last_rssi; }
  float getLastSNR() const override { return _last_snr; }
  int getNoiseFloor() const override { return _noise_floor; }
  void triggerNoiseFloorCalibrate(int threshold) override;
  void setCADEnabled(bool enable) override { _cad_enabled = enable; }
  void resetAGC() override;

  void setParams(float freq, float bw, uint8_t sf, uint8_t cr);
  void setTxPower(int8_t dbm);
  bool setRxBoostedGainMode(bool en);
  bool getRxBoostedGainMode() const { return _rx_boosted; }
  uint8_t getSpreadingFactor() const { return _sf; }
  float getCurrentRSSI();
  uint32_t getRngSeed();
  uint32_t getPacketsRecv() const { return n_recv; }
  uint32_t getPacketsSent() const { return n_sent; }
  uint32_t getPacketsRecvErrors() const { return n_recv_errors; }
  void resetStats() { n_recv = n_sent = n_recv_errors = 0; }
  bool configSideDetectors(const uint8_t *, uint8_t, float) { return false; }
};

#endif
