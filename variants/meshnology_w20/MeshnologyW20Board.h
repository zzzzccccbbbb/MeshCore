#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

// LoRa 3V3 enable (ME6231C33M5G), active HIGH
#ifndef P_LORA_EN
  #define P_LORA_EN 39
#endif

class MeshnologyW20Board : public ESP32Board {
public:
  void begin() {
    gpio_hold_dis((gpio_num_t)P_LORA_EN);
    pinMode(P_LORA_EN, OUTPUT);
    digitalWrite(P_LORA_EN, HIGH);

    /*
     * Do NOT gpio_config NSS/RST/BUSY here if Semtech HAL already owns SPI CS —
     * pinMode(NSS) after spi_bus_add_device steals CS and version reads become 255.255.
     * Pre-HAL idle levels are set in companion setup() / radio_init().
     */
#if !defined(W20_USE_SEMTECH_RADIO)
    pinMode(P_LORA_NSS, OUTPUT);
    digitalWrite(P_LORA_NSS, HIGH);
    pinMode(P_LORA_RESET, OUTPUT);
    digitalWrite(P_LORA_RESET, HIGH);
    pinMode(P_LORA_BUSY, INPUT_PULLUP);
    delay(200);
#endif

    ESP32Board::begin();

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
    digitalWrite(P_LORA_NSS, HIGH);
    digitalWrite(P_LORA_EN, LOW);

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
#if defined(W20_MESHNOLOGY)
    extern int w20_battery_read_vbat_mv(void);
    const int mv = w20_battery_read_vbat_mv();
    if (mv > 0)
      return (uint16_t)mv;
#endif
    return 0;
  }

  const char *getManufacturerName() const override {
    return "Meshnology W20";
  }
};
