#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

#define MCP23017_PIN_FLAG 0x8000U
#define MCP23017_PIN(n)   (MCP23017_PIN_FLAG | ((uint32_t)(n) & 0xFFU))
#define IS_MCP23017_PIN(pin) (((pin) & MCP23017_PIN_FLAG) != 0)
#define MCP23017_PIN_INDEX(pin) ((uint8_t)((pin) & 0xFFU))

class Mcp23017Helper {
  Adafruit_MCP23X17 _mcp;
  bool _ready = false;

public:
  bool begin(TwoWire &wire, uint8_t addr = 0x20);
  bool isReady() const { return _ready; }

  void powerOnPeripherals();

  void pinMode(uint8_t pin, uint8_t mode);
  void digitalWrite(uint8_t pin, uint8_t value);
  uint32_t digitalRead(uint8_t pin);

  static Mcp23017Helper &instance();
};
