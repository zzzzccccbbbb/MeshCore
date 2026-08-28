#pragma once

#include "DisplayDriver.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class MeshnologyW10Board;

class ST7796Display : public DisplayDriver {
  Adafruit_ST7789 display;
  MeshnologyW10Board* _board;
  bool _isOn;
  uint16_t _color;

  void lockSpi();
  void unlockSpi();

public:
  ST7796Display(MeshnologyW10Board* board);

  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};
