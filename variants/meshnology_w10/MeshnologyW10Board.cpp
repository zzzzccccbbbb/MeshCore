#include "MeshnologyW10Board.h"

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
  mcp.powerOnPeripherals();

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

uint16_t MeshnologyW10Board::getBattMilliVolts() {
#if defined(MESHNOLOGY_W10)
  if (_pmuReady && w10_pmu) {
    return (uint16_t)w10_pmu->getBattVoltage();
  }
#endif
  return 0;
}
