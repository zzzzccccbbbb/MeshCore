#include "W20ShellEpdDisplay.h"
#include "platform/w20_deckpro_shell/w20_deckpro_shell.h"

ColorVal UIColor::window_bkg = GxEPD_WHITE;
ColorVal UIColor::title_bkg = GxEPD_WHITE;
ColorVal UIColor::title_txt = GxEPD_BLACK;
ColorVal UIColor::primary_txt = GxEPD_BLACK;
ColorVal UIColor::secondary_txt = GxEPD_BLACK;
ColorVal UIColor::warning_txt = GxEPD_BLACK;
ColorVal UIColor::popup_bkg = GxEPD_WHITE;
ColorVal UIColor::popup_txt = GxEPD_BLACK;
ColorVal UIColor::corp_blue = GxEPD_BLACK;

/* 与 w20_meshtastic EInkDisplay2::forceDisplay（非 flip_screen）一致：只做 Y 镜像 blit */
#ifndef W20_MESHCORE_BLIT_MIRROR_Y
#define W20_MESHCORE_BLIT_MIRROR_Y 1
#endif

bool w20_shell_epd_flip_x(void) { return false; }
bool w20_shell_epd_flip_y(void) { return false; }
uint8_t w20_shell_epd_rotation(void) { return 0; }

static inline uint16_t canvas_color(ColorVal c)
{
  /* GxEPD：非白即黑；canvas1：非 0 即点亮(黑) */
  return (c == (ColorVal)GxEPD_WHITE) ? 0 : 1;
}

bool W20ShellEpdDisplay::begin()
{
  if (!w20_deckpro_shell_epd_reuse_ready()) {
    return false;
  }
  _epd = static_cast<DeckEpd *>(w20_deckpro_shell_epd_gx_bw_ptr());
  if (_epd == nullptr) {
    return false;
  }
  if (_canvas == nullptr) {
    _canvas = new GFXcanvas1(EINK_WIDTH, EINK_HEIGHT);
    if (_canvas == nullptr) {
      return false;
    }
  }
  _epd->setRotation(0);
  _epd->setPartialWindow(0, 0, EINK_WIDTH, EINK_HEIGHT);
  setTextSize(1);
  _isOn = true;
  return true;
}

void W20ShellEpdDisplay::restoreDeckRotation()
{
  if (_epd)
    _epd->setRotation(0);
}

void W20ShellEpdDisplay::turnOn()
{
  if (!_epd) begin();
  _isOn = true;
}

void W20ShellEpdDisplay::turnOff()
{
  _isOn = false;
}

void W20ShellEpdDisplay::clear()
{
  if (!_canvas) return;
  _canvas->fillScreen(0);
  _crc.reset();
}

void W20ShellEpdDisplay::startFrame(ColorVal bkg)
{
  if (!_canvas || !_epd) return;
  w20_deckpro_epd_lock();
  _canvas->fillScreen(canvas_color(bkg) ? 1 : 0);
  /* bkg 白=0；若传入黑底则整屏 1 */
  if (bkg == (ColorVal)GxEPD_WHITE) {
    _canvas->fillScreen(0);
  } else {
    _canvas->fillScreen(1);
  }
  _curr_color = 1;
  _canvas->setTextColor(_curr_color);
  _crc.reset();
}

void W20ShellEpdDisplay::setTextSize(int sz)
{
  if (!_canvas) return;
  _crc.update<int>(sz);
  switch (sz) {
    case 2:
      _canvas->setFont(&FreeSansBold12pt7b);
      break;
    case 3:
      _canvas->setFont(&FreeSans18pt7b);
      break;
    default:
      _canvas->setFont(&FreeSans9pt7b);
      break;
  }
}

void W20ShellEpdDisplay::setColor(ColorVal c)
{
  if (!_canvas) return;
  _crc.update<ColorVal>(c);
  _curr_color = canvas_color(c);
  _canvas->setTextColor(_curr_color);
}

void W20ShellEpdDisplay::setCursor(int x, int y)
{
  if (!_canvas) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _cursor_x = mapX(x);
  _cursor_y = mapY(y);
  _canvas->setCursor(_cursor_x, _cursor_y);
}

void W20ShellEpdDisplay::print(const char *str)
{
  if (!_canvas || !str) return;
  _crc.update<char>(str, strlen(str));
  _canvas->setCursor(_cursor_x, _cursor_y);
  _canvas->setTextColor(_curr_color);
  _canvas->print(str);
  _cursor_x = _canvas->getCursorX();
  _cursor_y = _canvas->getCursorY();
}

void W20ShellEpdDisplay::fillRect(int x, int y, int w, int h)
{
  if (!_canvas) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const int px = mapX(x);
  const int py = mapY(y);
  const int pw = (int)(w * scale_x);
  const int ph = (int)(h * scale_y);
  _canvas->fillRect(px, py, pw, ph, _curr_color);
}

void W20ShellEpdDisplay::drawRect(int x, int y, int w, int h)
{
  if (!_canvas) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const int px = mapX(x);
  const int py = mapY(y);
  const int pw = (int)(w * scale_x);
  const int ph = (int)(h * scale_y);
  _canvas->drawRect(px, py, pw, ph, _curr_color);
}

void W20ShellEpdDisplay::drawXbm(int x, int y, const uint8_t *bits, int w, int h)
{
  if (!_canvas || !bits) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const int startX = mapX(x);
  const int startY = mapY(y);
  const uint16_t widthInBytes = (w + 7) / 8;
  for (uint16_t by = 0; by < h; by++) {
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    if (block_h < 1)
      block_h = 1;
    for (uint16_t bx = 0; bx < w; bx++) {
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      if (block_w < 1)
        block_w = 1;
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      if (bits[byteOffset] & bitMask) {
        _canvas->fillRect(x1, y1, block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t W20ShellEpdDisplay::getTextWidth(const char *str)
{
  if (!_canvas || !str) return 0;
  int16_t x1, y1;
  uint16_t w, h;
  _canvas->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return (uint16_t)ceil((w + 1) / scale_x);
}

void W20ShellEpdDisplay::endFrame()
{
  if (!_epd || !_canvas || !_isOn) {
    if (_epd) w20_deckpro_epd_unlock();
    return;
  }

  const uint32_t crc = _crc.finalize();
  if (crc == (uint32_t)_last_crc) {
    w20_deckpro_epd_unlock();
    return;
  }

  /*
   * 对齐 w20_meshtastic EInkDisplay2 W20 分支：
   *   drawPixel(x, (H-1)-y)  — 与 Deck disp_flush（FLIP_Y=0）同向
   */
  _epd->setRotation(0);
  _epd->setFullWindow();
  _epd->firstPage();
  do {
    for (uint16_t y = 0; y < EINK_HEIGHT; y++) {
      for (uint16_t x = 0; x < EINK_WIDTH; x++) {
        const bool on = _canvas->getPixel(x, y);
#if W20_MESHCORE_BLIT_MIRROR_Y
        const uint16_t dy = (uint16_t)((EINK_HEIGHT - 1) - y);
#else
        const uint16_t dy = y;
#endif
        _epd->drawPixel((int16_t)x, (int16_t)dy, on ? GxEPD_BLACK : GxEPD_WHITE);
      }
    }
  } while (_epd->nextPage());

  w20_deckpro_epd_after_refresh();
  _last_crc = (int)crc;
  w20_deckpro_epd_unlock();
}
