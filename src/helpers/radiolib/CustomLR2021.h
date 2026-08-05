#pragma once

#include <RadioLib.h>
#include "MeshCore.h"

#ifndef LR2021_IRQ_DIO
  #define LR2021_IRQ_DIO 5
#endif

class CustomLR2021 : public LR2021 {
  bool _rx_boosted = false;

public:
  CustomLR2021(Module *mod) : LR2021(mod) {
    irqDioNum = LR2021_IRQ_DIO;
  }

  float getFreqMHz() const { return freqMHz; }

  uint8_t getSpreadingFactor() const { return spreadingFactor; }

  int16_t setRxBoostedGainMode(uint8_t level) {
    _rx_boosted = (level > 0);
    return LR2021::setRxBoostedGainMode(level);
  }

  bool getRxBoostedGainMode() const { return _rx_boosted; }

  bool isReceiving() {
    uint32_t irq = getIrqStatus();
#ifdef RADIOLIB_LR2021_IRQ_PREAMBLE_DETECTED
    bool detected = (irq & RADIOLIB_LR2021_IRQ_LORA_HEADER_VALID) ||
                    (irq & RADIOLIB_LR2021_IRQ_PREAMBLE_DETECTED);
#else
    bool detected = (irq & RADIOLIB_LR11X0_IRQ_SYNC_WORD_HEADER_VALID) ||
                    (irq & RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED);
#endif
    return detected;
  }

  // AM36-FN / LR2021MB1LDZCS switchless: DIO8/9 Sub-G TX/RX, DIO10/11 2.4G RX/TX
  void configureAm36RfSwitch() {
    static const uint32_t rfswitch_dio_pins[] = {
      RADIOLIB_LR2021_DIO8,
      RADIOLIB_LR2021_DIO9,
      RADIOLIB_LR2021_DIO10,
      RADIOLIB_LR2021_DIO11,
      RADIOLIB_NC,
    };

    static const Module::RfSwitchMode_t rfswitch_table[] = {
      {LR2021::MODE_STBY,  {LOW, LOW,  LOW,  LOW}},
      {LR2021::MODE_RX,    {LOW, HIGH, LOW,  LOW}},  // DIO9  = Sub-G RX
      {LR2021::MODE_TX,    {HIGH, LOW, LOW,  LOW}},  // DIO8  = Sub-G TX
      {LR2021::MODE_RX_HF, {LOW, LOW,  HIGH, LOW}},  // DIO10 = 2.4G RX
      {LR2021::MODE_TX_HF, {LOW, LOW,  LOW,  HIGH}}, // DIO11 = 2.4G TX
      END_OF_MODE_TABLE,
    };

    setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
  }

  // W12-MB external PA (w12_factory smtc_shield_lr2021mb1ldzcs):
  // DIO5=2.4G TXEN, DIO6=2.4G RXEN, DIO9=CTX, DIO10=CPS, DIO11=CSD
  void configureW12ExternalPaRfSwitch() {
    static const uint32_t rfswitch_dio_pins[] = {
      RADIOLIB_LR2021_DIO5,
      RADIOLIB_LR2021_DIO6,
      RADIOLIB_LR2021_DIO9,
      RADIOLIB_LR2021_DIO10,
      RADIOLIB_LR2021_DIO11,
    };

    static const Module::RfSwitchMode_t rfswitch_table[] = {
      //                  DIO5  DIO6  DIO9  DIO10 DIO11
      {LR2021::MODE_STBY,  {LOW,  LOW,  LOW,  LOW,  LOW}},
      {LR2021::MODE_RX,    {LOW,  LOW,  LOW,  LOW,  HIGH}}, // CSD on RX_LF
      {LR2021::MODE_TX,    {LOW,  LOW,  HIGH, HIGH, HIGH}}, // CTX+CPS+CSD on TX_LF
      {LR2021::MODE_RX_HF, {LOW,  HIGH, LOW,  LOW,  LOW}},  // 2.4G RXEN
      {LR2021::MODE_TX_HF, {HIGH, LOW,  LOW,  LOW,  LOW}},  // 2.4G TXEN
      END_OF_MODE_TABLE,
    };

    setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
  }

  bool std_init(SPIClass *spi = NULL) {
#if defined(ESP32) && defined(P_LORA_SCLK)
    if (spi) {
      spi->begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
    }
#else
    (void)spi;
#endif

    // XTAL module: tcxoVoltage must be 0 (non-zero causes -706/-707 on XTAL parts)
    tcxoVoltage = 0.0f;

    int status = begin(LORA_FREQ, LORA_BW, LORA_SF, 5,
                       RADIOLIB_LR2021_LORA_SYNC_WORD_PRIVATE,
                       LORA_TX_POWER, 16, 0.0f);
    if (status != RADIOLIB_ERR_NONE) {
      MESH_DEBUG_PRINTLN("LR2021: radio init failed: %d", status);
      return false;
    }

#ifdef LR2021_AM36_RF_SWITCH
    configureAm36RfSwitch();
#endif

#ifdef LR2021_W12_RF_SWITCH
    configureW12ExternalPaRfSwitch();
#endif

#if defined(LR2021_RX_BOOST_LEVEL)
    setRxBoostedGainMode(LR2021_RX_BOOST_LEVEL);
#endif

    setCRC(1);
    return true;
  }
};
