
#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

struct BleHandles {
  NimBLECharacteristic* chVoltage{nullptr};
  NimBLECharacteristic* chCurrent{nullptr};
  NimBLECharacteristic* chTemperature{nullptr};
  NimBLECharacteristic* chMode{nullptr};
};

class BleMgr {
public:
  void begin(const char* deviceName);
  void update(float V, float I, float T, const char* mode);
  bool clientConnected() const { return _clientConnected; }
  BleHandles& handles() { return _handles; }
private:
  friend class ServerCallbacks;
  bool _clientConnected{false};
  NimBLEServer* _server{nullptr};
  BleHandles _handles;
};