#pragma once

enum LoRaRadioType {
    NO_RADIO = 0,
    SX1262_RADIO,
    SX1268_RADIO,
    LR2021_RADIO,
};

extern LoRaRadioType radioType;
