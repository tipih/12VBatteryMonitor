
#include "ble_mgr.h"
#include <Preferences.h>

class CommandCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string value = pCharacteristic->getValue();
    Serial.printf("[BLE] Command received: %s\n", value.c_str());
    
    if (value == "CLEAR_NVM") {
      Serial.println("[BLE] Clearing all NVM storage...");
      Preferences prefs;
      
      // Clear battmon namespace (Rint learner data)
      prefs.begin("battmon", false);
      prefs.clear();
      prefs.end();
      Serial.println("[BLE]   - Cleared 'battmon' namespace");
      
      // Clear hall namespace (Hall sensor calibration)
      prefs.begin("hall", false);
      prefs.clear();
      prefs.end();
      Serial.println("[BLE]   - Cleared 'hall' namespace");
      
      pCharacteristic->setValue("NVM_CLEARED");
      pCharacteristic->notify();
      Serial.println("[BLE] NVM cleared successfully");
      
    } else if (value == "RESET") {
      Serial.println("[BLE] Resetting device in 2 seconds...");
      pCharacteristic->setValue("RESETTING");
      pCharacteristic->notify();
      delay(2000);
      ESP.restart();
      
    } else if (value == "CLEAR_RESET") {
      Serial.println("[BLE] Clearing NVM and resetting device...");
      Preferences prefs;
      prefs.begin("battmon", false);
      prefs.clear();
      prefs.end();
      prefs.begin("hall", false);
      prefs.clear();
      prefs.end();
      Serial.println("[BLE] NVM cleared, resetting in 2 seconds...");
      pCharacteristic->setValue("NVM_CLEARED_RESETTING");
      pCharacteristic->notify();
      delay(2000);
      ESP.restart();
      
    } else {
      Serial.printf("[BLE] Unknown command: %s\n", value.c_str());
      pCharacteristic->setValue("UNKNOWN_CMD");
      pCharacteristic->notify();
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
  explicit ServerCallbacks(BleMgr* mgr): _mgr(mgr) {}
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    _mgr->_clientConnected = true;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    _mgr->_clientConnected = false;
    Serial.println("[BLE] Client disconnected - advertising");
    NimBLEDevice::startAdvertising();
  }

  private:
  BleMgr* _mgr;
};

void BleMgr::begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  _server = NimBLEDevice::createServer();
  _server->setCallbacks(new ServerCallbacks(this));

  auto svc = _server->createService("a94f0001-12d3-11ee-be56-0242ac120002");
  _handles.chVoltage     = svc->createCharacteristic("a94f0002-12d3-11ee-be56-0242ac120002", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chCurrent     = svc->createCharacteristic("a94f0003-12d3-11ee-be56-0242ac120002", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chTemperature = svc->createCharacteristic("a94f0004-12d3-11ee-be56-0242ac120002", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chMode        = svc->createCharacteristic("a94f0005-12d3-11ee-be56-0242ac120002", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chCommand     = svc->createCharacteristic("a94f0006-12d3-11ee-be56-0242ac120002", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);

  _handles.chVoltage->createDescriptor("2901")->setValue("Battery Voltage (V)");
  _handles.chCurrent->createDescriptor("2901")->setValue("Battery Current (A)");
  _handles.chTemperature->createDescriptor("2901")->setValue("Battery Temperature (°C)");
  _handles.chMode->createDescriptor("2901")->setValue("Operating Mode");
  _handles.chCommand->createDescriptor("2901")->setValue("Command (CLEAR_NVM/RESET/CLEAR_RESET)");
  _handles.chCommand->setCallbacks(new CommandCallbacks());

  _handles.chVoltage->setValue("— V");
  _handles.chCurrent->setValue("— A");
  _handles.chTemperature->setValue("— °C");
  _handles.chMode->setValue("init");
  _handles.chCommand->setValue("READY");
  svc->start();

  NimBLEAdvertisementData advData; advData.setName(deviceName); advData.addServiceUUID("a94f0001-12d3-11ee-be56-0242ac120002");
  NimBLEAdvertisementData scanData; scanData.setName(deviceName);
  auto adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->setMinInterval(160);
  adv->setMaxInterval(240);
  NimBLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising started");
}

void BleMgr::update(float V, float I, float T, const char* mode) {
  if (!_clientConnected) return;
  char bufV[16], bufI[16], bufT[16];
  snprintf(bufV, sizeof(bufV), "%.2f V", V);
  snprintf(bufI, sizeof(bufI), "%.2f A", I);
  if (isfinite(T)) snprintf(bufT, sizeof(bufT), "%.1f °C", T); else snprintf(bufT, sizeof(bufT), "— °C");
  _handles.chVoltage->setValue(bufV);
  _handles.chCurrent->setValue(bufI);
  _handles.chTemperature->setValue(bufT);
  _handles.chMode->setValue(mode);
  _handles.chVoltage->notify();
  _handles.chCurrent->notify();
  _handles.chTemperature->notify();
  _handles.chMode->notify();
}
