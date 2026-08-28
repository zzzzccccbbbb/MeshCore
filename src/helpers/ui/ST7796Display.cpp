#include "ST7796Display.h"
#include <MeshnologyW10Board.h>

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 2
#endif

#ifndef DISPLAY_SCALE_X
  #define DISPLAY_SCALE_X 1.875f
#endif

#ifndef DISPLAY_SCALE_Y
  #define DISPLAY_SCALE_Y 3.75f
#endif

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

ColorVal UIColor::window_bkg = ST77XX_WHITE;
ColorVal UIColor::title_bkg = ST77XX_BLUE;
ColorVal UIColor::title_txt = ST77XX_WHITE;
ColorVal UIColor::primary_txt = ST77XX_BLACK;
ColorVal UIColor::secondary_txt = (18 << 11) | (36 << 5) | 18;
ColorVal UIColor::warning_txt = ST77XX_ORANGE;
ColorVal UIColor::popup_bkg = ST77XX_CYAN;
ColorVal UIColor::popup_txt = ST77XX_BLACK;
ColorVal UIColor::corp_blue = 0x001A;

ST7796Display::ST7796Display(MeshnologyW10Board* board)
  : DisplayDriver(128, 64),
    display(&board->sharedSpi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
    _board(board),
    _isOn(false) {}

void ST7796Display::lockSpi() {
  if (_board) {
    _board->lockSharedSpi();
  }
}

void ST7796Display::unlockSpi() {
  if (_board) {
    _board->unlockSharedSpi();
  }
}

bool ST7796Display::begin() {
  if (_isOn) {
    return true;
  }

  if (PIN_TFT_LEDA_CTL != -1) {
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
    digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
  }

  lockSpi();
  display.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  display.setRotation(DISPLAY_ROTATION);
  display.invertDisplay(true);
  display.setSPISpeed(40000000);
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2 * DISPLAY_SCALE_X);
  display.cp437(true);
  unlockSpi();

  _isOn = true;
  return true;
}

void ST7796Display::turnOn() {
  begin();
}

void ST7796Display::turnOff() {
  if (!_isOn) {
    return;
  }

  if (PIN_TFT_LEDA_CTL != -1) {
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
  }
  _isOn = false;
}

void ST7796Display::clear() {
  lockSpi();
  display.fillScreen(ST77XX_BLACK);
  unlockSpi();
}

void ST7796Display::startFrame(ColorVal bkg) {
  lockSpi();
  display.fillScreen(bkg);
  display.setTextColor(_color = UIColor::primary_txt);
  display.setTextSize(1 * DISPLAY_SCALE_X);
  display.cp437(true);
  unlockSpi();
}

void ST7796Display::setTextSize(int sz) {
  lockSpi();
  display.setTextSize(sz * DISPLAY_SCALE_X);
  unlockSpi();
}

void ST7796Display::setColor(ColorVal c) {
  _color = c;
  lockSpi();
  display.setTextColor(_color);
  unlockSpi();
}

void ST7796Display::setCursor(int x, int y) {
  lockSpi();
  display.setCursor(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y);
  unlockSpi();
}

void ST7796Display::print(const char* str) {
  lockSpi();
  display.print(str);
  unlockSpi();
}

void ST7796Display::fillRect(int x, int y, int w, int h) {
  lockSpi();
  display.fillRect(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y,
    w * DISPLAY_SCALE_X, h * DISPLAY_SCALE_Y, _color);
  unlockSpi();
}

void ST7796Display::drawRect(int x, int y, int w, int h) {
  lockSpi();
  display.drawRect(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y,
    w * DISPLAY_SCALE_X, h * DISPLAY_SCALE_Y, _color);
  unlockSpi();
}

void ST7796Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  uint8_t byteWidth = (w + 7) / 8;

  lockSpi();
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byte = bits[j * byteWidth + i / 8];
      bool pixelOn = byte & (0x80 >> (i & 7));

      if (pixelOn) {
        for (int dy = 0; dy < DISPLAY_SCALE_X; dy++) {
          for (int dx = 0; dx < DISPLAY_SCALE_X; dx++) {
            display.drawPixel(
              x * DISPLAY_SCALE_X + i * DISPLAY_SCALE_X + dx,
              y * DISPLAY_SCALE_Y + j * DISPLAY_SCALE_X + dy,
              _color);
          }
        }
      }
    }
  }
  unlockSpi();
}

uint16_t ST7796Display::getTextWidth(const char* str) {
  lockSpi();
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  unlockSpi();
  return w / DISPLAY_SCALE_X;
}

void ST7796Display::endFrame() {
}
