#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = -1;
#define BUILTIN_LED  LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

/* Default Arduino Wire — MeshCore overrides with PIN_BOARD_SDA/SCL (14/15). */
static const uint8_t SDA = 14;
static const uint8_t SCL = 15;

/* Default SPI aliases — LoRa/LCD shared bus. */
static const uint8_t SS    = 8;
static const uint8_t MOSI  = 17;
static const uint8_t MISO  = 11;
static const uint8_t SCK   = 18;

#endif /* Pins_Arduino_h */
