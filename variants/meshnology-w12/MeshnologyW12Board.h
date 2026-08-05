#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

// W12-MB: OLED VEXT and PA rails are active-low; ADC divider enable is active-high
#ifndef PIN_VEXT_EN
  #define PIN_VEXT_EN 45
#endif
#ifndef PIN_GSUB_POWER
  #define PIN_GSUB_POWER 4
#endif
#ifndef PIN_G24_POWER
  #define PIN_G24_POWER 3
#endif
#ifndef PIN_VBAT_READ
  #define PIN_VBAT_READ 1
#endif
#ifndef PIN_ADC_CTRL
  #define PIN_ADC_CTRL 2
#endif

class MeshnologyW12Board : public ESP32Board {
public:
  RefCountedDigitalPin periph_power;

  MeshnologyW12Board() : periph_power(PIN_VEXT_EN, LOW) {}

  void enablePaRails() {
    pinMode(PIN_GSUB_POWER, OUTPUT);
    pinMode(PIN_G24_POWER, OUTPUT);
    digitalWrite(PIN_GSUB_POWER, LOW);  // Sub-G PA on
    digitalWrite(PIN_G24_POWER, LOW);   // 2.4G PA on
    delay(200);  // match w12_factory power-up settle
  }

  void disablePaRails() {
    digitalWrite(PIN_GSUB_POWER, HIGH);
    digitalWrite(PIN_G24_POWER, HIGH);
  }

  void begin() {
    // PA rails before radio/I2C init (w12_factory order)
    enablePaRails();

    ESP32Board::begin();
    periph_power.begin();
    periph_power.claim();  // keep OLED rail powered

    pinMode(PIN_ADC_CTRL, OUTPUT);
    digitalWrite(PIN_ADC_CTRL, LOW);  // ADC divider off until sampled

    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
      long wakeup_source = esp_sleep_get_ext1_wakeup_status();
      if (wakeup_source & (1ULL << P_LORA_DIO_1)) {
        startup_reason = BD_STARTUP_RX_PACKET;
      }

      rtc_gpio_hold_dis((gpio_num_t)P_LORA_NSS);
      rtc_gpio_deinit((gpio_num_t)P_LORA_DIO_1);
    }
  }

  // Keep board-specific deep sleep: turn off external PA rails before sleep.
  // (Base ESP32Board::enterDeepSleep has no PA / DIO1-ext1 handling.)
  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1) {
    disablePaRails();
    periph_power.release();

    digitalWrite(P_LORA_NSS, HIGH);

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
      esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
    }

    esp_deep_sleep_start();
  }

  void powerOff() override {
    enterDeepSleep(0);
  }

  uint16_t getBattMilliVolts() override {
    // Divider 540k:100k → ×4.9 (w12_factory esp_battery_adc.h)
    digitalWrite(PIN_ADC_CTRL, HIGH);
    delay(5);

    uint32_t raw_mv = 0;
    for (int i = 0; i < 8; i++) {
      raw_mv += analogReadMilliVolts(PIN_VBAT_READ);
    }
    raw_mv /= 8;

    digitalWrite(PIN_ADC_CTRL, LOW);
    return (uint16_t)((raw_mv * 490UL) / 100UL);
  }

  const char *getManufacturerName() const override {
    return "Meshnology W12";
  }
};
