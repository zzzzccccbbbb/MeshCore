#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"
#include "MyMesh.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    newContactMessage,
    ack
};

class AbstractUITask : public MyMesh::Listener {
protected:
  mesh::MainBoard* _board;
  MultiSerialInterface* _interfaceManager;
  mutable uint16_t _cached_batt_mv;
  mutable unsigned long _next_batt_sample;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
    _cached_batt_mv = 0;
    _next_batt_sample = 0;
  }

public:
  bool hasConnection() const { return _interfaceManager->isConnected(); }
  /** Cached battery reading (~1.5s) so title bar / sensors pages stay consistent. */
  uint16_t getBattMilliVolts() const {
    unsigned long now = millis();
    if (_cached_batt_mv == 0 || (long)(now - _next_batt_sample) >= 0) {
      _cached_batt_mv = _board->getBattMilliVolts();
      _next_batt_sample = now + 1500;
    }
    return _cached_batt_mv;
  }
  bool isExternalPowered() const { return _board->isExternalPowered(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }

  virtual void notify(UIEventType t = UIEventType::none) = 0;
  virtual void loop() = 0;
};
