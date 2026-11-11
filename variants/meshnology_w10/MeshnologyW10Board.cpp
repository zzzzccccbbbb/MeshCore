#include "MeshnologyW10Board.h"

#ifndef W10_BATT_SPIKE_MV
  #define W10_BATT_SPIKE_MV 800
#endif

#if defined(MESHNOLOGY_W10)
  #include "XPowersLib.h"
  static XPowersAXP2101 *w10_pmu = nullptr;
#endif

void MeshnologyW10Board::lockSharedSpi() {
  if (_spiMutex) {
    xSemaphoreTake(_spiMutex, portMAX_DELAY);
  }
}

void MeshnologyW10Board::unlockSharedSpi() {
  if (_spiMutex) {
    xSemaphoreGive(_spiMutex);
  }
}

void MeshnologyW10Board::holdSharedSpiCsIdle() {
  /* Match factory: inserted SD/LCD must not drive MISO during LoRa SPI. */
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
}

void MeshnologyW10Board::initGpsPins() {
#if ENV_INCLUDE_GPS
  auto &mcp = Mcp23017Helper::instance();
  if (!mcp.isReady()) return;

  mcp.pinMode(MCP23017_PIN_INDEX(PIN_GPS_RESET), OUTPUT);
  mcp.pinMode(MCP23017_PIN_INDEX(PIN_GPS_EN), OUTPUT);
  mcp.digitalWrite(MCP23017_PIN_INDEX(PIN_GPS_RESET),
    PIN_GPS_RESET_ACTIVE == LOW ? HIGH : LOW);
  mcp.digitalWrite(MCP23017_PIN_INDEX(PIN_GPS_EN),
    PIN_GPS_EN_ACTIVE == HIGH ? HIGH : LOW);
#endif
}

void MeshnologyW10Board::initPmu() {
#if defined(MESHNOLOGY_W10)
  if (!w10_pmu) {
    w10_pmu = new XPowersAXP2101(Wire, PIN_BOARD_SDA, PIN_BOARD_SCL, I2C_PMU_ADD);
    if (!w10_pmu->init()) {
      delete w10_pmu;
      w10_pmu = nullptr;
      return;
    }
  }

  _pmuReady = true;
  w10_pmu->disableTSPinMeasure();
  w10_pmu->enableBattDetection();
  w10_pmu->enableBattVoltageMeasure();
  w10_pmu->enableVbusVoltageMeasure();
  w10_pmu->setDC2Voltage(1000);
  w10_pmu->enableDC2();
  w10_pmu->setDC3Voltage(3300);
  w10_pmu->enableDC3();
  w10_pmu->setDC4Voltage(1000);
  w10_pmu->enableDC4();
  w10_pmu->setDC5Voltage(3300);
  w10_pmu->enableDC5();
  w10_pmu->setALDO1Voltage(3300);
  w10_pmu->enableALDO1();
  w10_pmu->setALDO2Voltage(3300);
  w10_pmu->enableALDO2();
  w10_pmu->setBLDO1Voltage(1500);
  w10_pmu->enableBLDO1();
  w10_pmu->setBLDO2Voltage(2800);
  w10_pmu->enableBLDO2();
#endif
}

void MeshnologyW10Board::begin() {
  ESP32Board::begin();

  if (!_spiMutex) {
    _spiMutex = xSemaphoreCreateMutex();
  }

  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  Wire.setClock(400000);

  auto &mcp = Mcp23017Helper::instance();
  mcp.begin(Wire, 0x20);

  holdSharedSpiCsIdle();
  mcp.powerOnPeripherals();
  holdSharedSpiCsIdle();

  mcp.pinMode(MCP23017_PIN_INDEX(P_LORA_RESET), OUTPUT);
  mcp.pinMode(MCP23017_PIN_INDEX(P_LORA_DIO_1), INPUT);
  mcp.pinMode(MCP23017_PIN_INDEX(P_LORA_BUSY), INPUT);
  mcp.digitalWrite(MCP23017_PIN_INDEX(P_LORA_RESET), HIGH);

  initPmu();
  initGpsPins();

  sharedSpi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
}

void MeshnologyW10Board::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
  // DIO1 is on MCP23017 — packet wakeup via ext1 is not available.
  if (pin_wake_btn >= 0) {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)pin_wake_btn, 0);
  }
  if (secs > 0) {
    esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
  }
  esp_deep_sleep_start();
}

void MeshnologyW10Board::powerOff() {
  enterDeepSleep(0, PIN_USER_BTN);
}

bool MeshnologyW10Board::isExternalPowered() {
#if defined(MESHNOLOGY_W10)
  /* AXP2101: VBUS present or actively charging → show UI charge icon. */
  if (_pmuReady && w10_pmu) {
    return w10_pmu->isVbusIn() || w10_pmu->isCharging();
  }
#endif
  return false;
}

uint16_t MeshnologyW10Board::getBattMilliVolts() {
#if defined(MESHNOLOGY_W10)
  if (!_pmuReady || !w10_pmu || !w10_pmu->isBatteryConnect()) {
    return _batt_filt_ok ? (uint16_t)_batt_filt_mv : 0;
  }

  uint32_t mv = (uint32_t)w10_pmu->getBattVoltage();
  if (mv == 0) {
    return _batt_filt_ok ? (uint16_t)_batt_filt_mv : 0;
  }

  /* Light EMA + spike reject (same idea as M19 / factory). */
  if (!_batt_filt_ok || _batt_filt_mv == 0) {
    _batt_filt_mv = mv;
    _batt_filt_ok = true;
  } else {
    int32_t d = (int32_t)mv - (int32_t)_batt_filt_mv;
    if (d > W10_BATT_SPIKE_MV || d < -W10_BATT_SPIKE_MV) {
      return (uint16_t)_batt_filt_mv;
    }
    _batt_filt_mv = (_batt_filt_mv * 7 + mv) / 8;
  }
  return (uint16_t)_batt_filt_mv;
#else
  return 0;
#endif
}
