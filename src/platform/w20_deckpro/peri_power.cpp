/**
 * Sleep / Shutdown：外设断电、IP5356 VOUT2 关闭、EPD hibernate、GPIO 按原理图 park。
 */

#include "peri_power.h"
#include "utilities.h"
#include "peripheral.h"
#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)
#include "platform/w20_deckpro_shell/w20_deck_board_sleep.h"
#ifndef W20_DECK_SLEEP_EPD_HOLD_IMAGE
#define W20_DECK_SLEEP_EPD_HOLD_IMAGE 1
#endif
#endif
#include "ui_deckpro_port.h"
#include "ui_deckpro.h"

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#if USE_TOUCH
#include <TouchDrvCST3530.hpp>
extern TouchDrvCST3530 touch;
#endif

static void digital_safe_out_low(int pin)
{
    if (pin < 0)
        return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

static void digital_safe_out_high(int pin)
{
    if (pin < 0)
        return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

static void gpio_safe_reset(int pin)
{
    if (pin < 0)
        return;
    gpio_reset_pin((gpio_num_t)pin);
}

static void gpio_safe_hold(int pin)
{
    if (pin < 0)
        return;
    gpio_hold_en((gpio_num_t)pin);
}

static void pin_safe_input(int pin)
{
    if (pin < 0)
        return;
    pinMode(pin, INPUT);
}

static void pin_safe_input_pullup(int pin)
{
    if (pin < 0)
        return;
    pinMode(pin, INPUT_PULLUP);
}

static void power_disable_all_wake_sources(void)
{
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
#if defined(ESP_SLEEP_WAKEUP_TOUCHPAD)
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
#endif
#if defined(ESP_SLEEP_WAKEUP_ULP)
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ULP);
#endif
#if defined(ESP_SLEEP_WAKEUP_GPIO)
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
#endif
}

static void power_pause_ui_timers(void)
{
    ui_deckpro_pause_aux_timers();
    factory_pause_flush_timer();
}

static void power_rgb_off(void)
{
    if (PIN_RGB_R >= 0) {
        pinMode(PIN_RGB_R, OUTPUT);
        digitalWrite(PIN_RGB_R, RGB_R_ACTIVE_LEVEL ? LOW : HIGH);
    }
    if (PIN_RGB_B >= 0) {
        pinMode(PIN_RGB_B, OUTPUT);
        digitalWrite(PIN_RGB_B, RGB_B_ACTIVE_LEVEL ? LOW : HIGH);
    }
}

static void power_lighting_off(void)
{
#if (PIN_LIGHTING_LAMP >= 0)
    gpio_hold_dis((gpio_num_t)PIN_LIGHTING_LAMP);
    pinMode(PIN_LIGHTING_LAMP, OUTPUT);
    digitalWrite(PIN_LIGHTING_LAMP, LIGHTING_LAMP_OFF_LEVEL);
#endif
}

static void power_park_lora_pins(void)
{
    gpio_safe_reset(BOARD_LORA_SCK);
    gpio_safe_reset(BOARD_LORA_MOSI);
    gpio_safe_reset(BOARD_LORA_MISO);
    gpio_safe_reset(BOARD_LORA_CS);
    gpio_safe_reset(BOARD_LORA_RST);
    gpio_safe_reset(BOARD_LORA_BUSY);
    gpio_safe_reset(BOARD_LORA_INT);
#ifdef LORA_SCK
    gpio_safe_reset(LORA_SCK);
#endif
#ifdef LORA_MOSI
    gpio_safe_reset(LORA_MOSI);
#endif
#ifdef LORA_MISO
    gpio_safe_reset(LORA_MISO);
#endif
#ifdef LORA_CS
    gpio_safe_reset(LORA_CS);
#endif
#ifdef LORA_RESET
    gpio_safe_reset(LORA_RESET);
#endif
#ifdef LORA_DIO1
    gpio_safe_reset(LORA_DIO1);
#endif
#ifdef LORA_DIO2
    gpio_safe_reset(LORA_DIO2);
#endif
}

static void power_park_epd_pins(void)
{
#ifdef PIN_EINK_SCLK
    digital_safe_out_low(PIN_EINK_SCLK);
#endif
#ifdef PIN_EINK_MOSI
    digital_safe_out_low(PIN_EINK_MOSI);
#endif
#ifdef PIN_EINK_DC
    digital_safe_out_low(PIN_EINK_DC);
#endif
#ifdef PIN_EINK_CS
    digital_safe_out_high(PIN_EINK_CS);
#endif
#ifdef PIN_EINK_RES
    digital_safe_out_high(PIN_EINK_RES);
#endif
#ifdef PIN_EINK_BUSY
    /* 深睡后 BUSY 多为高阻，勿开内部上拉以免与屏侧电平对抗漏电 */
    pin_safe_input(PIN_EINK_BUSY);
#endif
}

static void power_park_gps_uart(void)
{
    gpio_safe_reset(BOARD_GPS_PPS);
    gpio_safe_reset(BOARD_GPS_RXD);
    gpio_safe_reset(BOARD_GPS_TXD);
}

static void power_park_always_on_gpios(void)
{
#if USE_TOUCH
    digital_safe_out_high(BOARD_TOUCH_RST);
    pin_safe_input(BOARD_TOUCH_INT);
#endif
    pin_safe_input(BOARD_I2C_SCL);
    pin_safe_input(BOARD_I2C_SDA);
#if (PIN_IP5356_INT >= 0)
    pin_safe_input(PIN_IP5356_INT);
#endif
    pin_safe_input(PIN_KEY_BOOT);
    pin_safe_input(PIN_KEY_LED);
#if (PIN_IP5356_KEY >= 0)
    pin_safe_input(PIN_IP5356_KEY);
#endif
}

static void power_peripherals_stop(void)
{
    WiFi.mode(WIFI_OFF);

#if USE_TOUCH
    touch.sleep();
    Serial.println("W20 shutdown: CST3530 touch sleep");
    Wire.end();
#endif

#if USE_LORA
    lora_sleep();
#endif

#if USE_GPS
    gps_task_suspend();
    SerialGPS.end();
#endif

    factory_epd_hibernate();
    factory_epd_release_bus();
    power_park_epd_pins();
    power_park_lora_pins();
    power_park_gps_uart();
    power_park_always_on_gpios();

    digital_safe_out_low(BOARD_GPS_WAKE);

    digital_safe_out_low(BOARD_6609_EN);
    digital_safe_out_low(BOARD_LORA_EN);
    digital_safe_out_low(BOARD_GPS_EN);
    digital_safe_out_low(BOARD_1V8_EN);
    digital_safe_out_low(BOARD_A7682E_PWRKEY);

    power_rgb_off();
    power_lighting_off();
}

static void power_hold_rails(void)
{
    gpio_safe_hold(BOARD_6609_EN);
    gpio_safe_hold(BOARD_LORA_EN);
    gpio_safe_hold(BOARD_GPS_EN);
    gpio_safe_hold(BOARD_1V8_EN);
    gpio_safe_hold(BOARD_A7682E_PWRKEY);
    gpio_safe_hold(BOARD_GPS_WAKE);
#if (PIN_LIGHTING_LAMP >= 0)
    gpio_safe_hold(PIN_LIGHTING_LAMP);
#endif
    if (PIN_RGB_R >= 0)
        gpio_safe_hold(PIN_RGB_R);
    if (PIN_RGB_B >= 0)
        gpio_safe_hold(PIN_RGB_B);
#if USE_TOUCH
    gpio_safe_hold(BOARD_TOUCH_RST);
#endif
#ifdef PIN_EINK_RES
    gpio_safe_hold(PIN_EINK_RES);
#endif
#ifdef PIN_EINK_CS
    gpio_safe_hold(PIN_EINK_CS);
#endif
#ifdef PIN_EINK_DC
    gpio_safe_hold(PIN_EINK_DC);
#endif
    gpio_deep_sleep_hold_en();
}

void power_deck_sleep_enter_periph(void)
{
#if USE_TOUCH
    /* 只睡触摸；勿 Wire.end()。软睡中主循环仍可能读 CW2217/IP5356，end 后会 NULL TX。 */
    touch.sleep();
#endif

#if W20_DECK_SLEEP_EPD_HOLD_IMAGE
    factory_epd_hold_image();
#else
    factory_epd_hibernate();
#endif
    factory_epd_release_bus();
    power_park_epd_pins();

#if USE_GPS
    if (BOARD_GPS_WAKE >= 0) {
        pinMode(BOARD_GPS_WAKE, OUTPUT);
        digitalWrite(BOARD_GPS_WAKE, LOW);
    }
    if (BOARD_GPS_EN >= 0) {
        pinMode(BOARD_GPS_EN, OUTPUT);
        digitalWrite(BOARD_GPS_EN, LOW);
    }
#endif
}

void power_deck_sleep_exit_periph(void)
{
#if USE_GPS
    if (BOARD_GPS_EN >= 0) {
        pinMode(BOARD_GPS_EN, OUTPUT);
        digitalWrite(BOARD_GPS_EN, HIGH);
    }
    if (BOARD_GPS_WAKE >= 0) {
        pinMode(BOARD_GPS_WAKE, OUTPUT);
        digitalWrite(BOARD_GPS_WAKE, HIGH);
    }
#endif

    (void)factory_epd_resume();

#if USE_TOUCH
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_INT);
    touch.wakeup();
#endif
}

void power_enter_sleep(void)
{
    power_pause_ui_timers();
    power_peripherals_stop();
    power_hold_rails();

    power_disable_all_wake_sources();
    esp_sleep_enable_ext1_wakeup((1ULL << (unsigned)BOARD_BOOT_PIN), ESP_EXT1_WAKEUP_ANY_LOW);

    esp_deep_sleep_start();
}

void power_shutdown_execute(void)
{
    Serial.println("Shutdown .....");
    power_pause_ui_timers();
    power_peripherals_stop();
    power_hold_rails();

#if (PIN_ESP_EN_KILL >= 0)
    pinMode(PIN_ESP_EN_KILL, OUTPUT);
    digitalWrite(PIN_ESP_EN_KILL, PIN_ESP_EN_KILL_ACTIVE_LEVEL ? HIGH : LOW);
    delay(100);
#endif

    power_disable_all_wake_sources();
    esp_deep_sleep_start();
}
