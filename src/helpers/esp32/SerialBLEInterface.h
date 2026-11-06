#pragma once

#include "../BaseSerialInterface.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class SerialBLEInterface : public BaseSerialInterface, BLESecurityCallbacks, BLEServerCallbacks, BLECharacteristicCallbacks {
  BLEServer *pServer;
  BLEService *pService;
  BLECharacteristic * pTxCharacteristic;
  bool deviceConnected;
  bool oldDeviceConnected;
  bool _isEnabled;
  bool _ble_inited;       /* begin() 成功且 controller 未 shutdown */
  bool _restore_enabled;  /* shutdown 前是否在广播，restore 时要不要 enable */
  uint16_t last_conn_id;
  uint32_t _pin_code;
  unsigned long _last_write;
  unsigned long adv_restart_time;
  char _ble_prefix[24];
  char _ble_name[32];     /* begin 解析后的名字（不含 prefix） */

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];
  };

  #define FRAME_QUEUE_SIZE  4
  StaticQueue_t recv_queue_state;
  uint8_t recv_queue_storage[FRAME_QUEUE_SIZE * sizeof(Frame)];
  QueueHandle_t recv_queue;
  int send_queue_len;
  Frame send_queue[FRAME_QUEUE_SIZE];

  void clearBuffers();

protected:
  // BLESecurityCallbacks methods
  uint32_t onPassKeyRequest() override;
  void onPassKeyNotify(uint32_t pass_key) override;
  bool onConfirmPIN(uint32_t pass_key) override;
  bool onSecurityRequest() override;
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override;

  // BLEServerCallbacks methods
  void onConnect(BLEServer* pServer) override;
  void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override;
  void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override;
  void onDisconnect(BLEServer* pServer) override;

  // BLECharacteristicCallbacks methods
  void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) override;

public:
  SerialBLEInterface() {
    pServer = NULL;
    pService = NULL;
    deviceConnected = false;
    oldDeviceConnected = false;
    adv_restart_time = 0;
    _isEnabled = false;
    _ble_inited = false;
    _restore_enabled = false;
    _last_write = 0;
    last_conn_id = 0;
    _ble_prefix[0] = 0;
    _ble_name[0] = 0;
    recv_queue = xQueueCreateStatic(
      FRAME_QUEUE_SIZE, sizeof(Frame), recv_queue_storage, &recv_queue_state
    );
    send_queue_len = 0;
  }

  /**
   * init the BLE interface.
   * @param prefix   a prefix for the device name
   * @param name  IN/OUT - a name for the device (combined with prefix). If "@@MAC" or "@@MAC4", is modified and returned
   * @param pin_code   the BLE security pin
   */
  void begin(const char* prefix, char* name, uint32_t pin_code);

  /**
   * WiFi/OTA：关掉 Bluedroid + BT controller（deinit false，可再 init）。
   * 之后可用 WIFI_PS_NONE；结束后调用 restoreController().
   */
  void shutdownController();
  /** 重新 begin +（若 shutdown 前在广播）enable。 */
  void restoreController();
  bool isControllerUp() const { return _ble_inited; }

  // BaseSerialInterface methods
  void enable() override;
  void disable() override;
  bool isEnabled() const override { return _isEnabled; }

  bool isConnected() const override;

  bool isWriteBusy() const override;
  size_t writeFrame(const uint8_t src[], size_t len) override;
  size_t checkRecvFrame(uint8_t dest[]) override;
};

#if BLE_DEBUG_LOGGING && ARDUINO
  #include <Arduino.h>
  #define BLE_DEBUG_PRINT(F, ...) Serial.printf("BLE: " F, ##__VA_ARGS__)
  #define BLE_DEBUG_PRINTLN(F, ...) Serial.printf("BLE: " F "\n", ##__VA_ARGS__)
#else
  #define BLE_DEBUG_PRINT(...) {}
  #define BLE_DEBUG_PRINTLN(...) {}
#endif
