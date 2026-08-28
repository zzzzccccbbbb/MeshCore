#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = -1;
#define BUILTIN_LED  LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 7;

static const uint8_t SS    = 14;
static const uint8_t MOSI  = 13;
static const uint8_t MISO  = 11;
static const uint8_t SCK   = 12;

#endif /* Pins_Arduino_h */
