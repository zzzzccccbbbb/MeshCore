#pragma once

#include <Arduino.h>
#include <helpers/RefCountedDigitalPin.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

// AM36-FN OLED VEXT is active-low
#ifndef PIN_VEXT_EN
  #define PIN_VEXT_EN 45
#endif

#ifndef PIN_VBAT_READ
  #define PIN_VBAT_READ 8   /* factory BOARD_BATTERY_ADC_GPIO */
#endif

#ifndef ADC_MULTIPLIER
  /* Factory divider 100k/100k → ×2.0 */
  #define ADC_MULTIPLIER 2.0f
#endif

class MeshnologyW15Board : public ESP32Board {
  float adc_mult = ADC_MULTIPLIER;
  uint32_t _batt_filt_mv = 0;
  bool _batt_filt_ok = false;

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

  uint16_t getBattMilliVolts() override;
  bool isExternalPowered() override;

  bool setAdcMultiplier(float multiplier) override {
    adc_mult = (multiplier == 0.0f) ? ADC_MULTIPLIER : multiplier;
    return true;
  }
  float getAdcMultiplier() const override { return adc_mult; }

  const char *getManufacturerName() const override {
    return "Meshnology W15";
  }
};
