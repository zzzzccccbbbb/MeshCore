#include "ST7789LCDDisplay.h"
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#if defined(MESHNOLOGY_M19)
  #include <MeshnologyM19Board.h>
#elif defined(MESHNOLOGY_W10)
  #include <MeshnologyW10Board.h>
#endif

#ifndef PIN_TFT_MISO
  #define PIN_TFT_MISO -1
#endif

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 3
#endif

#ifndef DISPLAY_SCALE_X
  #define DISPLAY_SCALE_X 2.5f // 320 / 128
#endif

#ifndef DISPLAY_SCALE_Y
  #define DISPLAY_SCALE_Y 3.75f // 240 / 64
#endif

/* Adafruit setTextSize() takes int — float scale must be rounded, not truncated.
 * M19 SCALE_X=1.875: size 1 used to become 1 (tiny); with round → 2. */
#ifndef DISPLAY_TEXT_SCALE
  #define DISPLAY_TEXT_SCALE DISPLAY_SCALE_X
#endif

static uint8_t scaledTextSize(int sz) {
  if (sz < 1) sz = 1;
  long n = lroundf((float)sz * (float)DISPLAY_TEXT_SCALE);
  if (n < 1) n = 1;
  if (n > 8) n = 8;
  return (uint8_t)n;
}

#ifndef ST7789_USE_CLASSIC_FONT
  #define ST7789_USE_FREESANS 1
#endif

#if ST7789_USE_FREESANS
void ST7789LCDDisplay::applyLogicalTextSize(int sz) {
  if (!_gfx) return;
  _text_sz = (sz < 1) ? 1 : sz;
  const GFXfont *font;
  if (_text_sz <= 1) {
    font = &FreeSans12pt7b;
  } else if (_text_sz == 2) {
    font = &FreeSans18pt7b;
  } else {
    font = &FreeSans24pt7b;
  }
  _gfx->setFont(font);
  _gfx->setTextSize(1);
  _gfx->setTextWrap(false);  /* right-align must not wrap at panel edge */
  /* FreeFonts use baseline Y; UI passes top-left — cache ascent. */
  int16_t x1, y1;
  uint16_t bw, bh;
  _gfx->getTextBounds("A", 0, 0, &x1, &y1, &bw, &bh);
  _font_ascent = (y1 < 0) ? (int16_t)(-y1) : (int16_t)font->yAdvance;
}
#else
void ST7789LCDDisplay::applyLogicalTextSize(int sz) {
  if (!_gfx) return;
  _text_sz = sz;
  _font_ascent = 0;
  _gfx->setFont(NULL);
  _gfx->setTextSize(scaledTextSize(sz));
  _gfx->setTextWrap(false);
}
#endif

#ifndef DISPLAY_WIDTH
  #define DISPLAY_WIDTH 240
#endif

#ifndef DISPLAY_HEIGHT
  #define DISPLAY_HEIGHT 320
#endif

#ifndef PIN_TFT_LEDA_CTL
  #define PIN_TFT_LEDA_CTL (-1)
#endif

/* ---- RGB332 back-buffer (physical panel size) ---- */

uint8_t ST7789FrameGFX::pack332(uint16_t c) {
  uint8_t r = (c >> 13) & 0x7;
  uint8_t g = (c >> 8) & 0x7;
  uint8_t b = (c >> 3) & 0x3;
  return (r << 5) | (g << 2) | b;
}

uint16_t ST7789FrameGFX::unpack565(uint8_t c) {
  uint8_t r = (c >> 5) & 0x7;
  uint8_t g = (c >> 2) & 0x7;
  uint8_t b = c & 0x3;
  /* Expand 3/3/2 → 5/6/5 */
  return (uint16_t)((r * 31 / 7) << 11) | (uint16_t)((g * 63 / 7) << 5) | (uint16_t)(b * 31 / 3);
}

ST7789FrameGFX::ST7789FrameGFX(int16_t w, int16_t h, uint8_t *buf)
  : Adafruit_GFX(w, h), _buf(buf), _stride(w) {}

void ST7789FrameGFX::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (!_buf || x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;
  _buf[(int32_t)y * _stride + x] = pack332(color);
}

void ST7789FrameGFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (!_buf || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > WIDTH) w = WIDTH - x;
  if (y + h > HEIGHT) h = HEIGHT - y;
  if (w <= 0 || h <= 0) return;

  uint8_t p = pack332(color);
  for (int16_t row = y; row < y + h; row++) {
    uint8_t *dst = _buf + (int32_t)row * _stride + x;
    memset(dst, p, (size_t)w);
  }
}

void ST7789FrameGFX::fillScreen(uint16_t color) {
  if (!_buf) return;
  memset(_buf, pack332(color), (size_t)_stride * HEIGHT);
}

void ST7789FrameGFX::blitTo(Adafruit_ST7789 &panel) {
  if (!_buf) return;
  const int16_t w = _stride;
  const int16_t h = HEIGHT;
  /* Max ST7789 panel width used by this driver (T-Deck 320). */
  uint16_t line[320];
  if (w > 320) return;

  panel.startWrite();
  for (int16_t y = 0; y < h; y++) {
    const uint8_t *src = _buf + (int32_t)y * w;
    for (int16_t x = 0; x < w; x++) {
      line[x] = unpack565(src[x]);
    }
    panel.setAddrWindow(0, y, w, 1);
    panel.writePixels(line, w);
  }
  panel.endWrite();
}

/* ---- Display driver ---- */

bool ST7789LCDDisplay::i2c_probe(TwoWire& wire, uint8_t addr) {
  return true;
}

#if defined(MESHNOLOGY_M19)
ST7789LCDDisplay::ST7789LCDDisplay(MeshnologyM19Board* board)
  : DisplayDriver(128, 64),
    display(&board->sharedSpi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
    _fb(NULL), _gfx(NULL), _color(ST77XX_WHITE), _text_sz(1), _font_ascent(0),
    _peripher_power(NULL), _m19_board(board)
{
  _isOn = false;
}

void ST7789LCDDisplay::lockSpi() {
  if (_m19_board) _m19_board->lockSharedSpi();
}

void ST7789LCDDisplay::unlockSpi() {
  if (_m19_board) _m19_board->unlockSharedSpi();
}
#elif defined(MESHNOLOGY_W10)
ST7789LCDDisplay::ST7789LCDDisplay(MeshnologyW10Board* board)
  : DisplayDriver(128, 64),
    display(&board->sharedSpi, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
    _fb(NULL), _gfx(NULL), _color(ST77XX_WHITE), _text_sz(1), _font_ascent(0),
    _peripher_power(NULL), _w10_board(board)
{
  _isOn = false;
}

void ST7789LCDDisplay::lockSpi() {
  if (_w10_board) _w10_board->lockSharedSpi();
}

void ST7789LCDDisplay::unlockSpi() {
  if (_w10_board) _w10_board->unlockSharedSpi();
}
#endif

// Color scheme
ColorVal UIColor::window_bkg = ST77XX_WHITE;
ColorVal UIColor::title_bkg = ST77XX_BLUE;
ColorVal UIColor::title_txt = ST77XX_WHITE;
ColorVal UIColor::primary_txt = ST77XX_BLACK;
ColorVal UIColor::secondary_txt = (18 << 11) | (36 << 5) | 18;  // mid-gray
ColorVal UIColor::warning_txt = ST77XX_ORANGE;
ColorVal UIColor::popup_bkg = ST77XX_CYAN;
ColorVal UIColor::popup_txt = ST77XX_BLACK;
ColorVal UIColor::corp_blue = 0x001A;

bool ST7789LCDDisplay::ensureFrameBuffer() {
  if (_gfx) return true;
  size_t nbytes = (size_t)DISPLAY_WIDTH * (size_t)DISPLAY_HEIGHT;
  _fb = (uint8_t *)malloc(nbytes);
  if (!_fb) return false;
  _gfx = new ST7789FrameGFX(DISPLAY_WIDTH, DISPLAY_HEIGHT, _fb);
  return _gfx != NULL;
}

bool ST7789LCDDisplay::begin() {
  if (!_isOn) {
    if (_peripher_power) _peripher_power->claim();

#if defined(MESHNOLOGY_M19)
    if (_m19_board) _m19_board->lcdResetPulse();
    if (PIN_TFT_LEDA_CTL != -1) {
      pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
      digitalWrite(PIN_TFT_LEDA_CTL, LOW);
    }
#else
    if (PIN_TFT_LEDA_CTL != -1) {
      pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
      digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
    }
#endif

    // Im not sure if this is just a t-deck problem or not, if your display is slow try this.
    #if defined(LILYGO_TDECK) || defined(HELTEC_LORA_V4_TFT) || defined(HELTEC_V4_R8_TFT)
      displaySPI.begin(PIN_TFT_SCL, PIN_TFT_MISO, PIN_TFT_SDA, PIN_TFT_CS);
    #elif defined(MESHNOLOGY_W13)
      SPI1.begin();
    #endif

#if defined(MESHNOLOGY_M19) || defined(MESHNOLOGY_W10)
    lockSpi();
#endif
    display.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    display.setRotation(DISPLAY_ROTATION);

    display.setSPISpeed(40e6);

#if defined(MESHNOLOGY_W13) || defined(MESHNOLOGY_M19) || defined(MESHNOLOGY_W10)
    /* Factory modules need inversion (matches factory INVON). */
    display.invertDisplay(true);
#endif

    if (!ensureFrameBuffer()) {
#if defined(MESHNOLOGY_M19) || defined(MESHNOLOGY_W10)
      unlockSpi();
#endif
      return false;
    }

    _gfx->fillScreen(ST77XX_BLACK);
    display.fillScreen(ST77XX_BLACK);
    _gfx->setTextColor(ST77XX_WHITE);
    applyLogicalTextSize(2);
#if !ST7789_USE_FREESANS
    _gfx->cp437(true);
#endif
#if defined(MESHNOLOGY_M19) || defined(MESHNOLOGY_W10)
    unlockSpi();
#endif
#if defined(MESHNOLOGY_M19)
    if (PIN_TFT_LEDA_CTL != -1) {
      digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
    }
#endif

    _isOn = true;
  }

  return true;
}

void ST7789LCDDisplay::turnOn() {
  ST7789LCDDisplay::begin();
}

void ST7789LCDDisplay::turnOff() {
  if (_isOn) {
#if !defined(MESHNOLOGY_M19)
    if (PIN_TFT_LEDA_CTL != -1) {
      digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
    }
#endif
    if (PIN_TFT_RST != -1) {
      digitalWrite(PIN_TFT_RST, LOW);
    }
    if (PIN_TFT_LEDA_CTL != -1) {
      digitalWrite(PIN_TFT_LEDA_CTL, LOW);
    }
    _isOn = false;

    if (_peripher_power) _peripher_power->release();
  }
}

void ST7789LCDDisplay::clear() {
  if (_gfx) _gfx->fillScreen(ST77XX_BLACK);
}

void ST7789LCDDisplay::startFrame(ColorVal bkg) {
  /* Clear back-buffer only — same layout math as direct draw, no live fillScreen flash. */
  if (!_gfx && !ensureFrameBuffer()) return;
  _gfx->fillScreen(bkg);
  _gfx->setTextColor(_color = UIColor::primary_txt);
  applyLogicalTextSize(1);
#if !ST7789_USE_FREESANS
  _gfx->cp437(true);
#endif
}

void ST7789LCDDisplay::setTextSize(int sz) {
  applyLogicalTextSize(sz);
}

void ST7789LCDDisplay::setColor(ColorVal c) {
  if (_gfx) _gfx->setTextColor(_color = c);
}

void ST7789LCDDisplay::setCursor(int x, int y) {
  if (!_gfx) return;
  int16_t px = (int16_t)lroundf((float)x * (float)DISPLAY_SCALE_X);
  int16_t py = (int16_t)lroundf((float)y * (float)DISPLAY_SCALE_Y);
#if ST7789_USE_FREESANS
  /* UI Y is top of text; FreeSans cursor is baseline. */
  py = (int16_t)(py + _font_ascent);
#endif
  _gfx->setCursor(px, py);
}

void ST7789LCDDisplay::print(const char* str) {
  if (_gfx) _gfx->print(str);
}

void ST7789LCDDisplay::fillRect(int x, int y, int w, int h) {
  if (_gfx) {
    _gfx->fillRect(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y,
                   w * DISPLAY_SCALE_X, h * DISPLAY_SCALE_Y, _color);
  }
}

void ST7789LCDDisplay::drawRect(int x, int y, int w, int h) {
  if (_gfx) {
    _gfx->drawRect(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y,
                   w * DISPLAY_SCALE_X, h * DISPLAY_SCALE_Y, _color);
  }
}

void ST7789LCDDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  if (!_gfx) return;
  uint8_t byteWidth = (w + 7) / 8;

  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint8_t byte = bits[j * byteWidth + i / 8];
      bool pixelOn = byte & (0x80 >> (i & 7));

      if (pixelOn) {
        for (int dy = 0; dy < DISPLAY_SCALE_X; dy++) {
          for (int dx = 0; dx < DISPLAY_SCALE_X; dx++) {
            _gfx->drawPixel(x * DISPLAY_SCALE_X + i * DISPLAY_SCALE_X + dx,
                            y * DISPLAY_SCALE_Y + j * DISPLAY_SCALE_X + dy, _color);
          }
        }
      }
    }
  }
}

uint16_t ST7789LCDDisplay::getTextWidth(const char* str) {
  if (!_gfx) return 0;
  int16_t x1, y1;
  uint16_t w, h;
  _gfx->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  /* Ceiling divide + left bearing so right-align cursor is not too far right. */
  int32_t phys = (int32_t)w + ((x1 < 0) ? (int32_t)(-x1) : 0);
  int32_t logical = (int32_t)ceilf((float)phys / (float)DISPLAY_SCALE_X);
  if (logical < 1 && str && str[0]) logical = 1;
  return (uint16_t)logical;
}

void ST7789LCDDisplay::endFrame() {
  if (_isOn && _gfx) {
#if defined(MESHNOLOGY_M19) || defined(MESHNOLOGY_W10)
    lockSpi();
    _gfx->blitTo(display);
    unlockSpi();
#else
    _gfx->blitTo(display);
#endif
  }
}
