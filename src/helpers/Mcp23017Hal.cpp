#include "Mcp23017Hal.h"

Mcp23017Hal::Mcp23017Hal(SPIClass &spi, SpiLockFn lockFn, SpiLockFn unlockFn, SPISettings settings)
  : ArduinoHal(spi, settings), _lockFn(lockFn), _unlockFn(unlockFn) {}

void Mcp23017Hal::spiBeginTransaction() {
  if (_lockFn) {
    _lockFn();
  }
  ArduinoHal::spiBeginTransaction();
}

void Mcp23017Hal::spiEndTransaction() {
  ArduinoHal::spiEndTransaction();
  if (_unlockFn) {
    _unlockFn();
  }
}

void Mcp23017Hal::pinMode(uint32_t pin, uint32_t mode) {
  if (IS_MCP23017_PIN(pin)) {
    uint8_t mcpPin = MCP23017_PIN_INDEX(pin);
    uint8_t mcpMode = (mode == GpioModeOutput) ? OUTPUT : INPUT;
    Mcp23017Helper::instance().pinMode(mcpPin, mcpMode);
    return;
  }
  ArduinoHal::pinMode(pin, mode);
}

void Mcp23017Hal::digitalWrite(uint32_t pin, uint32_t value) {
  if (IS_MCP23017_PIN(pin)) {
    Mcp23017Helper::instance().digitalWrite(MCP23017_PIN_INDEX(pin),
      value == GpioLevelHigh ? HIGH : LOW);
    return;
  }
  ArduinoHal::digitalWrite(pin, value);
}

uint32_t Mcp23017Hal::digitalRead(uint32_t pin) {
  if (IS_MCP23017_PIN(pin)) {
    return Mcp23017Helper::instance().digitalRead(MCP23017_PIN_INDEX(pin));
  }
  return ArduinoHal::digitalRead(pin);
}

void Mcp23017Hal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
  if (IS_MCP23017_PIN(interruptNum)) {
    // LoRa DIO1 is on MCP23017; packet IRQ uses polling in MeshCore.
    return;
  }
  ArduinoHal::attachInterrupt(interruptNum, interruptCb, mode);
}
