/*
 * Meshnology-W13 pin map (Arduino pin N == P0.N ; 32+N == P1.N)
 * Confirmed against W13-MB-V0.1 schematic / bring-up project.
 */

#pragma once

#include "WVariant.h"

#define VARIANT_MCK             (64000000ul)
#define USE_LFXO

#define PINS_COUNT              (48)
#define NUM_DIGITAL_PINS        (48)
#define NUM_ANALOG_INPUTS       (8)
#define NUM_ANALOG_OUTPUTS      (0)

/* ---- Mesh LED (P1.01) ---- */
#define PIN_LED                 (33)
#define LED_BLUE                (-1)
#define LED_BUILTIN             PIN_LED
#define P_LORA_TX_LED           PIN_LED
#define LED_STATE_ON            1

/* ---- Buttons / Joystick (active LOW) ---- */
#define PIN_BUTTON1             (8)   /* Menu / Back  P0.08 */
#define PIN_BUTTON2             (36)  /* Joy Up       P1.04 */
#define PIN_BUTTON3             (12)  /* Joy Down     P0.12 */
#define PIN_BUTTON4             (11)  /* Joy Left     P0.11 */
#define PIN_BUTTON5             (35)  /* Joy Right    P1.03 */
#define PIN_BUTTON6             (37)  /* Joy Press    P1.05 */
#define PIN_BACK_BTN            PIN_BUTTON1
#define JOYSTICK_UP             PIN_BUTTON2
#define JOYSTICK_DOWN           PIN_BUTTON3
#define JOYSTICK_LEFT           PIN_BUTTON4
#define JOYSTICK_RIGHT          PIN_BUTTON5
#define JOYSTICK_PRESS          PIN_BUTTON6
#define PIN_USER_BTN            PIN_BUTTON6

/* ---- Power / battery ---- */
#define VBAT_ENABLE             (4)   /* P0.04 BAT_ADC_CTR HIGH=measure */
#define PIN_VBAT_READ           (31)  /* P0.31 AIN7 */
#define AREF_VOLTAGE            (3.6F)
#define ADC_MULTIPLIER          (2.0F)
#define ADC_RESOLUTION          (12)

/* ---- Buzzer (PIN_BUZZER set per firmware env) ---- */
#define PIN_BUZZER_HW           (32)  /* P1.00 */

/* ---- I2C: SHT41 / BMP388 / OLED (SH1106) ---- */
#define WIRE_INTERFACES_COUNT   (1)
#define PIN_WIRE_SDA            (6)   /* P0.06 */
#define PIN_WIRE_SCL            (5)   /* P0.05 */
#ifndef DISPLAY_ADDRESS
#define DISPLAY_ADDRESS         0x3C
#endif

/* ---- GPS L76KB UART ---- */
#define PIN_SERIAL1_RX          (26)  /* MCU RX <- GNSS TX */
#define PIN_SERIAL1_TX          (27)  /* MCU TX -> GNSS RX */
#define GPS_BAUDRATE            9600
#define PIN_GPS_TX              PIN_SERIAL1_RX
#define PIN_GPS_RX              PIN_SERIAL1_TX
#define PIN_GPS_EN              (41)  /* P1.09 GNSS_WAKEUP */
#define PIN_GPS_RESET           (38)  /* P1.06 GNSS_RESET */
#define PIN_GPS_EN_ACTIVE       HIGH
#define PIN_GPS_RESET_ACTIVE    LOW

/* ---- LoRa E22-900MM22S (SX1262) on SPI0 ---- */
#define SPI_INTERFACES_COUNT    (2)

#define PIN_SPI_MISO            (3)   /* P0.03 */
#define PIN_SPI_MOSI            (28)  /* P0.28 */
#define PIN_SPI_SCK             (30)  /* P0.30 */

#define P_LORA_SCLK             PIN_SPI_SCK
#define P_LORA_MISO             PIN_SPI_MISO
#define P_LORA_MOSI             PIN_SPI_MOSI
#define P_LORA_NSS              (46)  /* P1.14 */
#define P_LORA_DIO_1            (7)   /* P0.07 */
#define P_LORA_RESET            (39)  /* P1.07 */
#define P_LORA_BUSY             (42)  /* P1.10 */
#define SX126X_DIO2_AS_RF_SWITCH true
#define SX126X_DIO3_TCXO_VOLTAGE (1.8f)

/* ---- QSPI Flash P25Q16SH ---- */
#define PIN_QSPI_SCK            (21)
#define PIN_QSPI_CS             (25)
#define PIN_QSPI_IO0            (20)
#define PIN_QSPI_IO1            (24)
#define PIN_QSPI_IO2            (22)
#define PIN_QSPI_IO3            (23)
#define EXTERNAL_FLASH_DEVICES  P25Q16H
#define EXTERNAL_FLASH_USE_QSPI

/* ---- E-Ink GDEY0213B74 on SPI1 (MOSI-only; dummy MISO) ---- */
#define PIN_DISPLAY_CS          (19)  /* P0.19 */
#define PIN_DISPLAY_BUSY        (17)  /* P0.17 */
#define PIN_DISPLAY_DC          (16)  /* P0.16 */
#define PIN_DISPLAY_RST         (14)  /* P0.14 */

#define PIN_SPI1_MISO           (2)   /* P0.02 header AIN0, not on FPC */
#define PIN_SPI1_MOSI           (15)  /* P0.15 */
#define PIN_SPI1_SCK            (13)  /* P0.13 */

/* GxEPD2 expects these globals on some boards */
extern const int MISO;
extern const int MOSI;
extern const int SCK;
