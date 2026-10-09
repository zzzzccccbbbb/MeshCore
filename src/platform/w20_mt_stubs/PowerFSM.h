#pragma once

enum {
    EVENT_PRESS = 1,
    EVENT_W20_DECK_SLEEP = 2,
    EVENT_WAKE_TIMER = 3,
};

class PowerFSM
{
  public:
    void trigger(int) {}
    void runOnce() {}
};

extern PowerFSM powerFSM;
