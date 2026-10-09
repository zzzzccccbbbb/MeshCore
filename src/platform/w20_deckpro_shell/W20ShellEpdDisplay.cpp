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

/*
 * Match Deck panel orientation without setRotation(2):
 * FLIP_Y aligns with Deck LVGL flush; FLIP_X corrects left-right mirror.
 */
#ifndef W20_MESHCORE_EPD_FLIP_Y
#define W20_MESHCORE_EPD_FLIP_Y 1
#endif
#ifndef W20_MESHCORE_EPD_FLIP_X
#define W20_MESHCORE_EPD_FLIP_X 1
#endif

static int s_log_x = 0;
static int s_log_y = 0;

static inline int epd_map_x(int x, int w = 0)
{
#if W20_MESHCORE_EPD_FLIP_X
  return EINK_WIDTH - x - w;
#else
  (void)w;
  return x;
#endif
}

static inline int epd_map_y(int y, int h = 0)
{
#if W20_MESHCORE_EPD_FLIP_Y
  return EINK_HEIGHT - y - h;
#else
  (void)h;
  return y;
#endif
}

bool W20ShellEpdDisplay::begin() {
  if (!w20_deckpro_shell_epd_reuse_ready()) {
    return false;
  }
  _epd = static_cast<DeckEpd *>(w20_deckpro_shell_epd_gx_bw_ptr());
  if (_epd == nullptr) {
    return false;
  }
  _epd->setRotation(0);
  setTextSize(1);
  _isOn = true;
  return true;
}

void W20ShellEpdDisplay::restoreDeckRotation() {
  if (_epd)
    _epd->setRotation(0);
}

void W20ShellEpdDisplay::turnOn() {
  if (!_epd) begin();
  _isOn = true;
}

void W20ShellEpdDisplay::turnOff() {
  _isOn = false;
}

void W20ShellEpdDisplay::clear() {
  if (!_epd) return;
  _epd->fillScreen(GxEPD_WHITE);
  _epd->setTextColor(GxEPD_BLACK);
  _crc.reset();
}

void W20ShellEpdDisplay::startFrame(ColorVal bkg) {
  if (!_epd) return;
  w20_deckpro_epd_lock();
  _epd->setFullWindow();
  _epd->fillScreen(bkg);
  _epd->setTextColor(_curr_color = UIColor::primary_txt);
  _crc.reset();
}

void W20ShellEpdDisplay::setTextSize(int sz) {
  if (!_epd) return;
  _crc.update<int>(sz);
  switch (sz) {
    case 2:
      _epd->setFont(&FreeSansBold12pt7b);
      break;
    case 3:
      _epd->setFont(&FreeSans18pt7b);
      break;
    default:
      _epd->setFont(&FreeSans9pt7b);
      break;
  }
}

void W20ShellEpdDisplay::setColor(ColorVal c) {
  if (!_epd) return;
  _crc.update<ColorVal>(c);
  _epd->setTextColor(_curr_color = c);
}

void W20ShellEpdDisplay::setCursor(int x, int y) {
  if (!_epd) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  s_log_x = (int)(x * scale_x);
  s_log_y = (int)(y * scale_y);
  /* Actual cursor applied in print() after text width is known (X flip). */
  _epd->setCursor(epd_map_x(s_log_x), epd_map_y(s_log_y));
}

void W20ShellEpdDisplay::print(const char *str) {
  if (!_epd || !str) return;
  _crc.update<char>(str, strlen(str));
#if W20_MESHCORE_EPD_FLIP_X
  int16_t x1, y1;
  uint16_t tw, th;
  _epd->getTextBounds(str, 0, 0, &x1, &y1, &tw, &th);
  _epd->setCursor(epd_map_x(s_log_x, (int)tw), epd_map_y(s_log_y));
  s_log_x += (int)tw; /* advance logical cursor like GFX */
#else
  _epd->setCursor(epd_map_x(s_log_x), epd_map_y(s_log_y));
#endif
  _epd->print(str);
}

void W20ShellEpdDisplay::fillRect(int x, int y, int w, int h) {
  if (!_epd) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const int px = (int)(x * scale_x);
  const int py = (int)(y * scale_y);
  const int pw = (int)(w * scale_x);
  const int ph = (int)(h * scale_y);
  _epd->fillRect(epd_map_x(px, pw), epd_map_y(py, ph), pw, ph, _curr_color);
}

void W20ShellEpdDisplay::drawRect(int x, int y, int w, int h) {
  if (!_epd) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const int px = (int)(x * scale_x);
  const int py = (int)(y * scale_y);
  const int pw = (int)(w * scale_x);
  const int ph = (int)(h * scale_y);
  _epd->drawRect(epd_map_x(px, pw), epd_map_y(py, ph), pw, ph, _curr_color);
}

void W20ShellEpdDisplay::drawXbm(int x, int y, const uint8_t *bits, int w, int h) {
  if (!_epd || !bits) return;
  _crc.update<int>(x);
  _crc.update<int>(y);
  _crc.update<int>(w);
  _crc.update<int>(h);
  const uint16_t startX = (uint16_t)(x * scale_x);
  const uint16_t startY = (uint16_t)(y * scale_y);
  const uint16_t widthInBytes = (w + 7) / 8;
  for (uint16_t by = 0; by < h; by++) {
    int y1 = startY + (int)(by * scale_y);
    int y2 = startY + (int)((by + 1) * scale_y);
    int block_h = y2 - y1;
    for (uint16_t bx = 0; bx < w; bx++) {
      int x1 = startX + (int)(bx * scale_x);
      int x2 = startX + (int)((bx + 1) * scale_x);
      int block_w = x2 - x1;
      uint16_t byteOffset = (by * widthInBytes) + (bx / 8);
      uint8_t bitMask = 0x80 >> (bx & 7);
      if (bits[byteOffset] & bitMask) {
        _epd->fillRect(epd_map_x(x1, block_w), epd_map_y(y1, block_h), block_w, block_h, _curr_color);
      }
    }
  }
}

uint16_t W20ShellEpdDisplay::getTextWidth(const char *str) {
  if (!_epd || !str) return 0;
  int16_t x1, y1;
  uint16_t w, h;
  _epd->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return (uint16_t)ceil((w + 1) / scale_x);
}

void W20ShellEpdDisplay::endFrame() {
  if (!_epd || !_isOn) {
    if (_epd) w20_deckpro_epd_unlock();
    return;
  }
  uint32_t crc = _crc.finalize();
  if (crc != (uint32_t)_last_crc) {
    _epd->display(false); // DEPG0310: full refresh only
    w20_deckpro_epd_after_refresh();
    _last_crc = (int)crc;
  }
  w20_deckpro_epd_unlock();
}
