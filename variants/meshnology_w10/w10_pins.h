#pragma once

#include <helpers/Mcp23017Helper.h>

// LoRa SPI (shared with LCD)
#ifndef P_LORA_SCLK
  #define P_LORA_SCLK 12
#endif
#ifndef P_LORA_MISO
  #define P_LORA_MISO 11
#endif
#ifndef P_LORA_MOSI
  #define P_LORA_MOSI 13
#endif
#ifndef P_LORA_NSS
  #define P_LORA_NSS 14
#endif

// LoRa control via MCP23017 (EXIO3/9/10)
#ifndef P_LORA_RESET
  #define P_LORA_RESET MCP23017_PIN(3)
#endif
#ifndef P_LORA_DIO_1
  #define P_LORA_DIO_1 MCP23017_PIN(9)
#endif
#ifndef P_LORA_BUSY
  #define P_LORA_BUSY MCP23017_PIN(10)
#endif

#ifndef PIN_BOARD_SDA
  #define PIN_BOARD_SDA 8
#endif
#ifndef PIN_BOARD_SCL
  #define PIN_BOARD_SCL 7
#endif

#ifndef PIN_USER_BTN
  #define PIN_USER_BTN 0
#endif

// ST7796 3.5" 240x240 square LCD (SPI2 shared with LoRa)
#ifndef PIN_TFT_CS
  #define PIN_TFT_CS 10
#endif
#ifndef PIN_TFT_DC
  #define PIN_TFT_DC 16
#endif
#ifndef PIN_TFT_RST
  #define PIN_TFT_RST -1
#endif
#ifndef PIN_TFT_LEDA_CTL
  #define PIN_TFT_LEDA_CTL 6
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

// GPS L76K
#ifndef PIN_GPS_RX
  #define PIN_GPS_RX 44
#endif
#ifndef PIN_GPS_TX
  #define PIN_GPS_TX 43
#endif
#ifndef PIN_GPS_RESET
  #define PIN_GPS_RESET MCP23017_PIN(11)
#endif
#ifndef PIN_GPS_EN
  #define PIN_GPS_EN MCP23017_PIN(12)
#endif
#ifndef PIN_GPS_RESET_ACTIVE
  #define PIN_GPS_RESET_ACTIVE LOW
#endif
#ifndef PIN_GPS_EN_ACTIVE
  #define PIN_GPS_EN_ACTIVE HIGH
#endif

#ifndef I2C_PMU_ADD
  #define I2C_PMU_ADD 0x34
#endif
