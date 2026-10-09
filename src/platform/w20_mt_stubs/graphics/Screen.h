#pragma once

#include "OLEDDisplay.h"
#include <stdint.h>

namespace graphics
{

class Screen
{
  public:
    void forceDisplay(bool = false) {}
    void stopBootScreen() {}
    void w20PrepareMeshUiFrame() {}
    void w20DrainPendingCmds() {}
    void w20CancelOverlayNavEpdFlush() {}
    void w20PollOverlayNavEpdFlush() {}
    void w20SyncSetOn(bool) {}
    void setOn(bool) {}
    OLEDDisplay *getDisplayDevice() { return nullptr; }
};

} // namespace graphics

extern graphics::Screen *screen;
