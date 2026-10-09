#pragma once

namespace graphics
{

class NotificationRenderer
{
  public:
    static bool isOverlayBannerShowing() { return false; }
    static void resetBanner() {}
};

} // namespace graphics
