#pragma once

#include <helpers/Mcp23017Helper.h>

/*
 * Meshnology M19 pin map (from factory board_pins.h / MCP23017).
 * MCP23017 @0x20: PIN0..7 = GPA0..7, PIN8..15 = GPB0..7
 */

/* ---- I2C (MCP23017 + camera SCCB) ---- */
#ifndef PIN_BOARD_SDA
  #define PIN_BOARD_SDA 14
#endif
#ifndef PIN_BOARD_SCL
  #define PIN_BOARD_SCL 15
#endif

/* ---- LoRa SPI (shared with LCD / SD) ---- */
#ifndef P_LORA_SCLK
  #define P_LORA_SCLK 18
#endif
#ifndef P_LORA_MISO
  #define P_LORA_MISO 11
#endif
#ifndef P_LORA_MOSI
  #define P_LORA_MOSI 17
#endif
#ifndef P_LORA_NSS
  #define P_LORA_NSS 8
#endif
#ifndef P_LORA_BUSY
  #define P_LORA_BUSY 12
#endif
/* ESP GPIO38 ← LR2021 DIO7 IRQ */
#ifndef P_LORA_DIO_1
  #define P_LORA_DIO_1 38
#endif
/* LoRa hard reset on MCP GPB4 */
#ifndef P_LORA_RESET
  #define P_LORA_RESET MCP23017_PIN(12)
#endif

/* MCP power / PA (Board::begin owns sequencing) */
#ifndef P_LORA_PO_EN
  #define P_LORA_PO_EN MCP23017_PIN(11)       /* GPB3, active-high */
#endif
#ifndef P_LORA_LF_PA_POWER
  #define P_LORA_LF_PA_POWER MCP23017_PIN(9)  /* GPB1 PA_EN_M Sub-G */
#endif
#ifndef P_LORA_HF_PA_POWER
  #define P_LORA_HF_PA_POWER MCP23017_PIN(8)  /* GPB0 PA_EN_G 2.4G */
#endif
#ifndef PIN_VEXT_EN
  #define PIN_VEXT_EN MCP23017_PIN(10)        /* GPB2 LCD/SD/RGB, active-low */
#endif
#ifndef PIN_VEXT_EN_ACTIVE
  #define PIN_VEXT_EN_ACTIVE LOW
#endif
#ifndef PIN_LCD_RST
  #define PIN_LCD_RST MCP23017_PIN(13)        /* GPB5 */
#endif
#ifndef PIN_ADC_CTRL
  #define PIN_ADC_CTRL MCP23017_PIN(7)        /* GPA7, active-high */
#endif

/* Board-managed GNSS (do NOT pass these to MicroNMEA — MCP pins). */
#ifndef M19_GPS_EN
  #define M19_GPS_EN MCP23017_PIN(6)          /* GPA6, active-low (P-MOS) */
#endif
#ifndef M19_GPS_RST
  #define M19_GPS_RST MCP23017_PIN(5)         /* GPA5 */
#endif
#ifndef M19_GPS_WKUP
  #define M19_GPS_WKUP MCP23017_PIN(4)        /* GPA4 */
#endif

/* UART GPS L76KB — ESP RX←mod TX, ESP TX→mod RX */
#ifndef PIN_GPS_RX
  #define PIN_GPS_RX 44
#endif
#ifndef PIN_GPS_TX
  #define PIN_GPS_TX 43
#endif

#ifndef PIN_USER_BTN
  #define PIN_USER_BTN 0
#endif
#ifndef PIN_BACK_BTN
  #define PIN_BACK_BTN 0   /* BOOT key; used as back when UI_HAS_JOYSTICK */
#endif
#ifndef PIN_KEY_ADC
  #define PIN_KEY_ADC 13   /* five-way M1-5246G ladder */
#endif

#ifndef PIN_VBAT_READ
  #define PIN_VBAT_READ 1
#endif

#ifndef NEOPIXEL_LED
  #define NEOPIXEL_LED 46
#endif

/* ST7789 240x240 — shared SPI with LoRa; RST via MCP (PIN_TFT_RST=-1). */
#ifndef PIN_TFT_CS
  #define PIN_TFT_CS 21
#endif
#ifndef PIN_TFT_DC
  #define PIN_TFT_DC 6
#endif
#ifndef PIN_TFT_RST
  #define PIN_TFT_RST -1
#endif
#ifndef PIN_TFT_LEDA_CTL
  #define PIN_TFT_LEDA_CTL 5
#endif
#ifndef PIN_TFT_SCL
  #define PIN_TFT_SCL P_LORA_SCLK
#endif
#ifndef PIN_TFT_SDA
  #define PIN_TFT_SDA P_LORA_MOSI
#endif
#ifndef PIN_TFT_MISO
  #define PIN_TFT_MISO P_LORA_MISO
#endif
