#pragma once

class RadioInterface
{
  public:
    virtual ~RadioInterface() {}
    virtual bool init() { return false; }
    virtual void disable() {}
};

extern RadioInterface *rIf;
