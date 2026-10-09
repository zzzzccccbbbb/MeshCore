#pragma once

#include <stdint.h>

enum input_broker_event {
    INPUT_BROKER_NONE = 0,
    INPUT_BROKER_USER_PRESS,
    INPUT_BROKER_SELECT,
    INPUT_BROKER_SHUTDOWN,
    INPUT_BROKER_LEFT,
    INPUT_BROKER_RIGHT,
    INPUT_BROKER_UP,
    INPUT_BROKER_DOWN,
};

struct InputEvent {
    const char *source;
    input_broker_event inputEvent;
    uint16_t touchX;
    uint16_t touchY;
};

class InputBroker
{
  public:
    void injectInputEvent(const InputEvent *) {}
    void registerSource(void *) {}
};

extern InputBroker *inputBroker;
