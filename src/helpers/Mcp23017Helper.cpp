#include "Mcp23017Helper.h"

Mcp23017Helper &Mcp23017Helper::instance() {
  static Mcp23017Helper helper;
  return helper;
}

bool Mcp23017Helper::begin(TwoWire &wire, uint8_t addr) {
  _ready = _mcp.begin_I2C(addr, &wire);
  return _ready;
}

void Mcp23017Helper::powerOnPeripherals() {
  if (!_ready) return;
  pinMode(1, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(1, LOW);
  digitalWrite(13, LOW);
  delay(100);
  digitalWrite(1, HIGH);
  digitalWrite(13, HIGH);
  delay(100);
}

void Mcp23017Helper::pinMode(uint8_t pin, uint8_t mode) {
  if (!_ready) return;
  _mcp.pinMode(pin, mode);
}

void Mcp23017Helper::digitalWrite(uint8_t pin, uint8_t value) {
  if (!_ready) return;
  _mcp.digitalWrite(pin, value);
}

uint32_t Mcp23017Helper::digitalRead(uint8_t pin) {
  if (!_ready) return 0;
  return _mcp.digitalRead(pin) ? 1 : 0;
}
