#include "ST7796Display.h"
#include <MeshnologyW10Board.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <math.h>

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 2
#endif

#ifndef DISPLAY_SCALE_X
  #define DISPLAY_SCALE_X 1.875f
#endif

#ifndef DISPLAY_SCALE_Y
  #define DISPLAY_SCALE_Y 3.75f
#endif

#ifndef DISPLAY_TEXT_SCALE
  #define DISPLAY_TEXT_SCALE DISPLAY_SCALE_X
#endif

#ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH 240
#endif

#ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT 240
#endif

#ifndef ST7796_USE_CLASSIC_FONT
  #define ST7796_USE_FREESANS 1
#endif

static uint8_t scaledTextSize(int sz) {
  if (sz < 1) sz = 1;
  long n = lroundf((float)sz * (float)DISPLAY_TEXT_SCALE);
  if (n < 1) n = 1;
  if (n > 8) n = 8;
  return (uint8_t)n;
}

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
    _isOn(false),
    _text_sz(1),
    _font_ascent(0) {}

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

#if ST7796_USE_FREESANS
void ST7796Display::applyLogicalTextSize(int sz) {
  _text_sz = (sz < 1) ? 1 : sz;
  const GFXfont *font;
  if (_text_sz <= 1) {
    font = &FreeSans12pt7b;
  } else if (_text_sz == 2) {
    font = &FreeSans18pt7b;
  } else {
    font = &FreeSans24pt7b;
  }
  display.setFont(font);
  display.setTextSize(1);
  display.setTextWrap(false);
  int16_t x1, y1;
  uint16_t bw, bh;
  display.getTextBounds("A", 0, 0, &x1, &y1, &bw, &bh);
  _font_ascent = (y1 < 0) ? (int16_t)(-y1) : (int16_t)font->yAdvance;
}
#else
void ST7796Display::applyLogicalTextSize(int sz) {
  _text_sz = sz;
  _font_ascent = 0;
  display.setFont(NULL);
  display.setTextSize(scaledTextSize(sz));
  display.setTextWrap(false);
}
#endif

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
  applyLogicalTextSize(2);
#if !ST7796_USE_FREESANS
  display.cp437(true);
#endif
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
  applyLogicalTextSize(1);
#if !ST7796_USE_FREESANS
  display.cp437(true);
#endif
  unlockSpi();
}

void ST7796Display::setTextSize(int sz) {
  lockSpi();
  applyLogicalTextSize(sz);
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
  int16_t px = (int16_t)lroundf((float)x * (float)DISPLAY_SCALE_X);
  int16_t py = (int16_t)lroundf((float)y * (float)DISPLAY_SCALE_Y);
#if ST7796_USE_FREESANS
  py = (int16_t)(py + _font_ascent);
#endif
  display.setCursor(px, py);
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
  int32_t phys = (int32_t)w + ((x1 < 0) ? (int32_t)(-x1) : 0);
  int32_t logical = (int32_t)ceilf((float)phys / (float)DISPLAY_SCALE_X);
  if (logical < 1 && str && str[0]) logical = 1;
  return (uint16_t)logical;
}

void ST7796Display::endFrame() {
}
