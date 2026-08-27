#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

// AM36-FN OLED VEXT is active-low
#ifndef PIN_VEXT_EN
  #define PIN_VEXT_EN 45
#endif

class MeshnologyW15Board : public ESP32Board {
public:
  RefCountedDigitalPin periph_power;

  MeshnologyW15Board() : periph_power(PIN_VEXT_EN, LOW) {}

  void begin() {
    ESP32Board::begin();
    periph_power.begin();
    periph_power.claim();  // keep OLED rail powered

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

  // Keep DIO1 ext1 wakeup; base ESP32Board::enterDeepSleep is timer-only.
  void enterDeepSleep(uint32_t secs, int pin_wake_btn = -1) {
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
    return 0;  // battery ADC not wired in current W15 firmware
  }

  const char *getManufacturerName() const override {
    return "Meshnology W15";
  }
};
