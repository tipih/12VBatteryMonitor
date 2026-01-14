
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

struct BleHandles {
  NimBLECharacteristic *chVoltage{nullptr};
  NimBLECharacteristic *chCurrent{nullptr};
  NimBLECharacteristic *chTemperature{nullptr};
  NimBLECharacteristic *chMode{nullptr};
  NimBLECharacteristic *chCommand{nullptr};
  NimBLECharacteristic *chSOC{nullptr};
  NimBLECharacteristic *chSOH{nullptr};
  NimBLECharacteristic *chCapacity{nullptr};
  NimBLECharacteristic *chStatus{nullptr};
  NimBLECharacteristic *chAhLeft{nullptr};
};

class BleMgr {
public:
  void begin(const char *deviceName);
  void update(float V, float I, float T, const char *mode, float soc_pct,
              float soh_pct, float ah_left);
  enum PendingCommand : uint8_t {
    CMD_NONE = 0,
    CMD_CLEAR_NVM,
    CMD_RESET,
    CMD_CLEAR_RESET,
    CMD_SET_CAP,
    CMD_SET_BASE,
    CMD_NVS_TEST
  };
  void enqueueCommand(PendingCommand cmd, float param = NAN) {
    _pendingCommand = (uint8_t)cmd;
    _pendingParam = param;
  }
  void process();
  bool clientConnected() const { return _clientConnected; }
  BleHandles &handles() { return _handles; }

private:
  friend class ServerCallbacks;
  bool _clientConnected{false};
  NimBLEServer *_server{nullptr};
  BleHandles _handles;
  volatile uint8_t _pendingCommand{CMD_NONE};
  volatile float _pendingParam{NAN};
};

// Extern global instance (defined in main.cpp)
extern BleMgr ble;

// Helper to notify a short status string over the command characteristic
void ble_notify_status(const char *msg);