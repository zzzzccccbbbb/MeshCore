#include "MeshnologyM19Board.h"
#include "HWCDC.h"

#ifndef M19_BATT_SAMPLES
  #define M19_BATT_SAMPLES 16
#endif
#ifndef M19_BATT_SPIKE_MV
  #define M19_BATT_SPIKE_MV 800
#endif

void MeshnologyM19Board::mcpPinMode(uint32_t mcpPin, uint8_t mode) {
  auto &mcp = Mcp23017Helper::instance();
  if (!mcp.isReady()) return;
  mcp.pinMode(MCP23017_PIN_INDEX(mcpPin), mode);
}

void MeshnologyM19Board::mcpWrite(uint32_t mcpPin, uint8_t level) {
  auto &mcp = Mcp23017Helper::instance();
  if (!mcp.isReady()) return;
  mcp.digitalWrite(MCP23017_PIN_INDEX(mcpPin), level);
}

void MeshnologyM19Board::lockSharedSpi() {
  if (_spiMutex) {
    xSemaphoreTake(_spiMutex, portMAX_DELAY);
  }
}

void MeshnologyM19Board::unlockSharedSpi() {
  if (_spiMutex) {
    xSemaphoreGive(_spiMutex);
  }
}

void MeshnologyM19Board::powerRailsOn() {
  /* Safe defaults first (match factory exio_mcp23017_init). */
  mcpWrite(PIN_VEXT_EN, HIGH);          /* active-low → off */
  mcpWrite(M19_GPS_EN, HIGH);           /* active-low → off */
  mcpWrite(P_LORA_PO_EN, LOW);
  mcpWrite(P_LORA_LF_PA_POWER, LOW);
  mcpWrite(P_LORA_HF_PA_POWER, LOW);
  mcpWrite(PIN_ADC_CTRL, LOW);
  mcpWrite(P_LORA_RESET, HIGH);
  mcpWrite(PIN_LCD_RST, HIGH);
  mcpWrite(M19_GPS_RST, HIGH);
  mcpWrite(M19_GPS_WKUP, HIGH);
  delay(10);

  /* esp_power_init: VEXT → LORA_PO → Sub-G PA on, 2.4G PA off */
  mcpWrite(PIN_VEXT_EN, PIN_VEXT_EN_ACTIVE);  /* LOW = on */
  delay(50);
  mcpWrite(P_LORA_PO_EN, HIGH);
  mcpWrite(P_LORA_LF_PA_POWER, HIGH);
  mcpWrite(P_LORA_HF_PA_POWER, LOW);
  delay(100);
}

void MeshnologyM19Board::initGpsPins() {
#if ENV_INCLUDE_GPS
  setGpsPower(true);
#endif
}

void MeshnologyM19Board::setGpsPower(bool on) {
  auto &mcp = Mcp23017Helper::instance();
  if (!mcp.isReady()) return;

  mcpPinMode(M19_GPS_EN, OUTPUT);
  mcpPinMode(M19_GPS_RST, OUTPUT);
  mcpPinMode(M19_GPS_WKUP, OUTPUT);

  if (on) {
    mcpWrite(M19_GPS_EN, LOW);     /* P-MOS: low = 3V3 on */
    mcpWrite(M19_GPS_WKUP, HIGH);
    mcpWrite(M19_GPS_RST, HIGH);
    delay(200);
    /* Hardware reset pulse */
    mcpWrite(M19_GPS_RST, LOW);
    delay(10);
    mcpWrite(M19_GPS_RST, HIGH);
  } else {
    mcpWrite(M19_GPS_EN, HIGH);
    mcpWrite(M19_GPS_RST, LOW);
  }
}

void MeshnologyM19Board::lcdResetPulse() {
  mcpPinMode(PIN_LCD_RST, OUTPUT);
  mcpWrite(PIN_LCD_RST, LOW);
  delay(20);
  mcpWrite(PIN_LCD_RST, HIGH);
  delay(120);
}

void MeshnologyM19Board::holdSharedSpiCsIdle() {
  /* Match factory: inserted SD must not drive MISO during LoRa SPI. */
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
}

void MeshnologyM19Board::begin() {
  ESP32Board::begin();

  if (!_spiMutex) {
    _spiMutex = xSemaphoreCreateMutex();
  }

  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  Wire.setClock(400000);

  auto &mcp = Mcp23017Helper::instance();
  mcp.begin(Wire, 0x20);

  mcpPinMode(PIN_VEXT_EN, OUTPUT);
  mcpPinMode(P_LORA_PO_EN, OUTPUT);
  mcpPinMode(P_LORA_LF_PA_POWER, OUTPUT);
  mcpPinMode(P_LORA_HF_PA_POWER, OUTPUT);
  mcpPinMode(P_LORA_RESET, OUTPUT);
  mcpPinMode(PIN_LCD_RST, OUTPUT);
  mcpPinMode(PIN_ADC_CTRL, OUTPUT);
  mcpPinMode(M19_SD_PWDN, OUTPUT);
  mcpPinMode(M19_GPS_EN, OUTPUT);
  mcpPinMode(M19_GPS_RST, OUTPUT);
  mcpPinMode(M19_GPS_WKUP, OUTPUT);

  /* MeshCore does not mount SD — keep powered down like factory default. */
  mcpWrite(M19_SD_PWDN, HIGH);

  holdSharedSpiCsIdle();
  powerRailsOn();
  holdSharedSpiCsIdle();
  initGpsPins();

  sharedSpi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);

  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_DEEPSLEEP) {
    long wakeup_source = esp_sleep_get_ext1_wakeup_status();
    if (wakeup_source & (1LL << P_LORA_DIO_1)) {
      startup_reason = BD_STARTUP_RX_PACKET;
    }
    rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
    rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
  }
}

void MeshnologyM19Board::onBeforeTransmit(void) {
#ifdef NEOPIXEL_LED
  #ifndef NEOPIXEL_BRIGHTNESS
    #define NEOPIXEL_BRIGHTNESS 64
  #endif
  neopixelWrite(NEOPIXEL_LED, NEOPIXEL_BRIGHTNESS, NEOPIXEL_BRIGHTNESS, NEOPIXEL_BRIGHTNESS);
#endif
}

void MeshnologyM19Board::onAfterTransmit(void) {
#ifdef NEOPIXEL_LED
  neopixelWrite(NEOPIXEL_LED, 0, 0, 0);
#endif
}

void MeshnologyM19Board::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
  /* Drop peripheral rails before sleep. */
  if (Mcp23017Helper::instance().isReady()) {
    mcpWrite(P_LORA_HF_PA_POWER, LOW);
    mcpWrite(P_LORA_LF_PA_POWER, LOW);
    mcpWrite(PIN_ADC_CTRL, LOW);
    mcpWrite(M19_GPS_EN, HIGH);
    mcpWrite(PIN_VEXT_EN, HIGH);   /* VEXT off */
    /* Keep LoRa powered optionally for DIO wake — hold NSS, wake on DIO7/GPIO38 */
  }

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  rtc_gpio_set_direction((gpio_num_t)P_LORA_DIO_1, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pulldown_en((gpio_num_t)P_LORA_DIO_1);
  rtc_gpio_hold_en((gpio_num_t)P_LORA_NSS);

  if (pin_wake_btn < 0) {
    esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1), ESP_EXT1_WAKEUP_ANY_HIGH);
  } else {
    esp_sleep_enable_ext1_wakeup((1ULL << P_LORA_DIO_1) | (1ULL << pin_wake_btn),
                                 ESP_EXT1_WAKEUP_ANY_HIGH);
  }

  if (secs > 0) {
    esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
  }

  esp_deep_sleep_start();
}

void MeshnologyM19Board::powerOff() {
  enterDeepSleep(0);
}

bool MeshnologyM19Board::isExternalPowered() {
  /* ESP32-S3 USB-Serial/JTAG: host plugged ≈ VBUS present (same cable that charges). */
  return HWCDC::isPlugged();
}

uint16_t MeshnologyM19Board::getBattMilliVolts() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  mcpWrite(PIN_ADC_CTRL, HIGH);
  delay(10);

  uint32_t pin_mv_sum = 0;
  for (int i = 0; i < M19_BATT_SAMPLES; i++) {
    pin_mv_sum += (uint32_t)analogReadMilliVolts(PIN_VBAT_READ);
  }
  mcpWrite(PIN_ADC_CTRL, LOW);

  uint32_t pin_mv = pin_mv_sum / M19_BATT_SAMPLES;
  /* Divider 390k/100k (+ factory one-point gain baked into adc_mult ≈ 5.28). */
  uint32_t mv = (uint32_t)((float)pin_mv * adc_mult);

  /* EMA + spike reject (same idea as factory esp_battery_adc). */
  if (!_batt_filt_ok || _batt_filt_mv == 0) {
    _batt_filt_mv = mv;
    _batt_filt_ok = true;
  } else {
    int32_t d = (int32_t)mv - (int32_t)_batt_filt_mv;
    if (d > M19_BATT_SPIKE_MV || d < -M19_BATT_SPIKE_MV) {
      return (uint16_t)_batt_filt_mv;
    }
    _batt_filt_mv = (_batt_filt_mv * 7 + mv) / 8;
  }
  return (uint16_t)_batt_filt_mv;
}
