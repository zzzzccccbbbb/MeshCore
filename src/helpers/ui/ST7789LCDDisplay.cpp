#include "ST7789LCDDisplay.h"
#include <stdlib.h>
#include <string.h>

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

    if (PIN_TFT_LEDA_CTL != -1) {
      pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
      digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
    }

    // Im not sure if this is just a t-deck problem or not, if your display is slow try this.
    #if defined(LILYGO_TDECK) || defined(HELTEC_LORA_V4_TFT) || defined(HELTEC_V4_R8_TFT)
      displaySPI.begin(PIN_TFT_SCL, PIN_TFT_MISO, PIN_TFT_SDA, PIN_TFT_CS);
    #elif defined(MESHNOLOGY_W13)
      SPI1.begin();
    #endif

    display.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    display.setRotation(DISPLAY_ROTATION);

    display.setSPISpeed(40e6);

#if defined(MESHNOLOGY_W13)
    /* Factory 1.54" modules need inversion (matches factory INVON). */
    display.invertDisplay(true);
#endif

    if (!ensureFrameBuffer()) {
      return false;
    }

    _gfx->fillScreen(ST77XX_BLACK);
    display.fillScreen(ST77XX_BLACK);
    _gfx->setTextColor(ST77XX_WHITE);
    _gfx->setTextSize(2 * DISPLAY_SCALE_X);
    _gfx->cp437(true);

    _isOn = true;
  }

  return true;
}

void ST7789LCDDisplay::turnOn() {
  ST7789LCDDisplay::begin();
}

void ST7789LCDDisplay::turnOff() {
  if (_isOn) {
    if (PIN_TFT_LEDA_CTL != -1) {
      digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
    }
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
  _gfx->setTextSize(1 * DISPLAY_SCALE_X);
  _gfx->cp437(true);
}

void ST7789LCDDisplay::setTextSize(int sz) {
  if (_gfx) _gfx->setTextSize(sz * DISPLAY_SCALE_X);
}

void ST7789LCDDisplay::setColor(ColorVal c) {
  if (_gfx) _gfx->setTextColor(_color = c);
}

void ST7789LCDDisplay::setCursor(int x, int y) {
  if (_gfx) _gfx->setCursor(x * DISPLAY_SCALE_X, y * DISPLAY_SCALE_Y);
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
  return w / DISPLAY_SCALE_X;
}

void ST7789LCDDisplay::endFrame() {
  if (_isOn && _gfx) {
    _gfx->blitTo(display);
  }
}
