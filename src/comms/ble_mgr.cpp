
#include "ble_mgr.h"
#include "../learner/rint_learner.h"
#include <Preferences.h>
#include <app_config.h>
#include <comms/mqtt_mgr.h>

// mqtt instance declared in main.cpp
extern MqttMgr mqtt;

class CommandCallbacks : public NimBLECharacteristicCallbacks {
public:
  explicit CommandCallbacks(BleMgr *mgr) : _mgr(mgr) {}
  void onWrite(NimBLECharacteristic *pCharacteristic,
               NimBLEConnInfo &connInfo) override {
    std::string value = pCharacteristic->getValue();
    DBG_PRINTF("[BLE] Command received (queued): %s\n", value.c_str());
    // Make a case-insensitive copy for command matching
    std::string u = value;
    for (auto &ch : u)
      ch = (char)toupper((unsigned char)ch);
    // Check for SET_CAP command with a numeric parameter
    if (u.rfind("SET_CAP", 0) == 0) {
      // Accept formats: "SET_CAP:12.5" or "SET_CAP 12.5" or "SET_CAP=12.5"
      size_t pos = value.find_first_of(": =", 7);
      std::string num;
      if (pos != std::string::npos)
        num = value.substr(pos + 1);
      else if (value.size() > 7)
        num = value.substr(7);
      float v = NAN;
      if (!num.empty())
        v = atof(num.c_str());
      if (isfinite(v) && v > 0.0f) {
        _mgr->enqueueCommand(BleMgr::CMD_SET_CAP, v);
        pCharacteristic->setValue("QUEUED_SET_CAP");
        pCharacteristic->notify();
      } else {
        pCharacteristic->setValue("BAD_PARAM");
        pCharacteristic->notify();
      }
      return;
    }

    // Check for SET_BASE or SET_BASELINE to set Rint baseline (mOhm)
    if (u.rfind("SET_BASE", 0) == 0 || u.rfind("SET_BASELINE", 0) == 0) {
      size_t pos = value.find_first_of(": =", 9);
      std::string num;
      if (pos != std::string::npos)
        num = value.substr(pos + 1);
      else if (value.size() > 9)
        num = value.substr(9);
      float v = NAN;
      if (!num.empty())
        v = atof(num.c_str());
      if (isfinite(v) && v > 0.0f) {
        _mgr->enqueueCommand(BleMgr::CMD_SET_BASE, v);
        pCharacteristic->setValue("QUEUED_SET_BASE");
        pCharacteristic->notify();
      } else {
        pCharacteristic->setValue("BAD_PARAM");
        pCharacteristic->notify();
      }
      return;
    }

    if (u == "CLEAR_NVM") {
      _mgr->enqueueCommand(BleMgr::CMD_CLEAR_NVM);
      pCharacteristic->setValue("QUEUED_CLEAR_NVM");
      pCharacteristic->notify();
    } else if (u == "NVS_TEST") {
      _mgr->enqueueCommand(BleMgr::CMD_NVS_TEST);
      pCharacteristic->setValue("QUEUED_NVS_TEST");
      pCharacteristic->notify();
    } else if (u == "RESET") {
      _mgr->enqueueCommand(BleMgr::CMD_RESET);
      pCharacteristic->setValue("QUEUED_RESET");
      pCharacteristic->notify();
    } else if (u == "CLEAR_RESET") {
      _mgr->enqueueCommand(BleMgr::CMD_CLEAR_RESET);
      pCharacteristic->setValue("QUEUED_CLEAR_RESET");
      pCharacteristic->notify();
    }
    // NOTE: processing of queued commands (including NVS_TEST) occurs
    // in BleMgr::process() running in the main loop. Avoid doing heavy
    // work inside the NimBLE callback context.
    else {
      DBG_PRINTF("[BLE] Unknown command (queued): %s\n", value.c_str());
      pCharacteristic->setValue("UNKNOWN_CMD");
      pCharacteristic->notify();
    }
  }

private:
  BleMgr *_mgr;
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
  explicit ServerCallbacks(BleMgr *mgr) : _mgr(mgr) {}
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
    _mgr->_clientConnected = true;
    DBG_PRINTLN("[BLE] Client connected");
  }
  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo,
                    int reason) override {
    _mgr->_clientConnected = false;
    DBG_PRINTLN("[BLE] Client disconnected - advertising");
    NimBLEDevice::startAdvertising();
  }

private:
  BleMgr *_mgr;
};

void BleMgr::begin(const char *deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  _server = NimBLEDevice::createServer();
  _server->setCallbacks(new ServerCallbacks(this));

  auto svc = _server->createService("a94f0001-12d3-11ee-be56-0242ac120002");
  _handles.chVoltage = svc->createCharacteristic(
      "a94f0002-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chCurrent = svc->createCharacteristic(
      "a94f0003-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chTemperature = svc->createCharacteristic(
      "a94f0004-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chMode = svc->createCharacteristic(
      "a94f0005-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chCommand = svc->createCharacteristic(
      "a94f0006-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  _handles.chSOC = svc->createCharacteristic(
      "a94f0007-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chSOH = svc->createCharacteristic(
      "a94f0008-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chCapacity = svc->createCharacteristic(
      "a94f0009-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chStatus = svc->createCharacteristic(
      "a94f000a-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  _handles.chAhLeft = svc->createCharacteristic(
      "a94f000b-12d3-11ee-be56-0242ac120002",
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  _handles.chVoltage->createDescriptor("2901")->setValue("Battery Voltage (V)");
  _handles.chCurrent->createDescriptor("2901")->setValue("Battery Current (A)");
  _handles.chTemperature->createDescriptor("2901")->setValue(
      "Battery Temperature (°C)");
  _handles.chMode->createDescriptor("2901")->setValue("Operating Mode");
  _handles.chCommand->createDescriptor("2901")->setValue(
      "Command (CLEAR_NVM/RESET/CLEAR_RESET/SET_CAP:<Ah>)");
  _handles.chSOC->createDescriptor("2901")->setValue("State of Charge (%)");
  _handles.chSOH->createDescriptor("2901")->setValue("State of Health (%)");
  _handles.chCapacity->createDescriptor("2901")->setValue(
      "Battery Capacity (Ah)");
  _handles.chStatus->createDescriptor("2901")->setValue("Device Status");
  _handles.chAhLeft->createDescriptor("2901")->setValue("Ah Left (Ah)");
  _handles.chCommand->setCallbacks(new CommandCallbacks(this));

  _handles.chVoltage->setValue("— V");
  _handles.chCurrent->setValue("— A");
  _handles.chTemperature->setValue("— °C");
  _handles.chMode->setValue("init");
  _handles.chCommand->setValue("READY");
  _handles.chSOC->setValue("— %");
  _handles.chSOH->setValue("— %");
  _handles.chCapacity->setValue("— Ah");
  _handles.chStatus->setValue("OK");
  _handles.chAhLeft->setValue("— Ah");
  svc->start();

  NimBLEAdvertisementData advData;
  advData.setName(deviceName);
  advData.addServiceUUID("a94f0001-12d3-11ee-be56-0242ac120002");
  NimBLEAdvertisementData scanData;
  scanData.setName(deviceName);
  auto adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->setMinInterval(160);
  adv->setMaxInterval(240);
  NimBLEDevice::startAdvertising();
  DBG_PRINTLN("[BLE] Advertising started");
}

void BleMgr::update(float V, float I, float T, const char *mode, float soc_pct,
                    float soh_pct, float ah_left) {
  if (!_clientConnected)
    return;
  char bufV[16], bufI[16], bufT[16], bufSOC[16], bufSOH[16];
  snprintf(bufV, sizeof(bufV), "%.2f V", V);
  snprintf(bufI, sizeof(bufI), "%.2f A", I);
  if (isfinite(T))
    snprintf(bufT, sizeof(bufT), "%.1f °C", T);
  else
    snprintf(bufT, sizeof(bufT), "— °C");
  if (isfinite(soc_pct))
    snprintf(bufSOC, sizeof(bufSOC), "%.1f %%", soc_pct);
  else
    snprintf(bufSOC, sizeof(bufSOC), "— %%");
  if (isfinite(soh_pct))
    snprintf(bufSOH, sizeof(bufSOH), "%.1f %%", soh_pct);
  else
    snprintf(bufSOH, sizeof(bufSOH), "— %%");
  _handles.chVoltage->setValue(bufV);
  _handles.chCurrent->setValue(bufI);
  _handles.chTemperature->setValue(bufT);
  _handles.chMode->setValue(mode);
  _handles.chSOC->setValue(bufSOC);
  _handles.chSOH->setValue(bufSOH);
  _handles.chVoltage->notify();
  _handles.chCurrent->notify();
  _handles.chTemperature->notify();
  _handles.chMode->notify();
  _handles.chSOC->notify();
  _handles.chSOH->notify();
  if (_handles.chCapacity) {
    char bufCap[16];
    snprintf(bufCap, sizeof(bufCap), "%.2f Ah", batteryCapacityAh);
    _handles.chCapacity->setValue(bufCap);
    _handles.chCapacity->notify();
  }
  if (_handles.chAhLeft) {
    char bufAh[16];
    if (isfinite(ah_left))
      snprintf(bufAh, sizeof(bufAh), "%.2f Ah", ah_left);
    else
      snprintf(bufAh, sizeof(bufAh), "— Ah");
    _handles.chAhLeft->setValue(bufAh);
    _handles.chAhLeft->notify();
  }
}

void BleMgr::process() {
  // Also run a lightweight maintenance loop for advertising even when no
  // command is pending. This helps recover advertising if the stack stops
  // advertising after a failed connection attempt.
  static uint32_t lastAdvRestartMs = 0;
  uint32_t nowMs = millis();
  if (!_clientConnected) {
    if ((uint32_t)(nowMs - lastAdvRestartMs) >= 5000) {
      DBG_PRINTLN("[BLE] Maintenance: ensuring advertising");
      NimBLEDevice::startAdvertising();
      lastAdvRestartMs = nowMs;
    }
  }

  if (_pendingCommand == CMD_NONE)
    return;
  uint8_t cmd = _pendingCommand;
  _pendingCommand = CMD_NONE;

  if (!_handles.chCommand)
    return;

  if (cmd == CMD_CLEAR_NVM) {
    DBG_PRINTLN("[BLE] Processing CLEAR_NVM (main loop)...");
    Preferences prefs;
    prefs.begin("battmon", false);
    prefs.clear();
    prefs.end();
    DBG_PRINTLN("[BLE]   - Cleared 'battmon' namespace");
    prefs.begin("hall", false);
    prefs.clear();
    prefs.end();
    DBG_PRINTLN("[BLE]   - Cleared 'hall' namespace");
    _handles.chCommand->setValue("NVM_CLEARED");
    _handles.chCommand->notify();
  } else if (cmd == CMD_RESET) {
    DBG_PRINTLN("[BLE] Processing RESET (main loop)...");
    _handles.chCommand->setValue("RESETTING");
    _handles.chCommand->notify();
    delay(2000);
    ESP.restart();
  } else if (cmd == CMD_CLEAR_RESET) {
    DBG_PRINTLN("[BLE] Processing CLEAR_RESET (main loop)...");
    Preferences prefs;
    prefs.begin("battmon", false);
    prefs.clear();
    prefs.end();
    prefs.begin("hall", false);
    prefs.clear();
    prefs.end();
    _handles.chCommand->setValue("NVM_CLEARED_RESETTING");
    _handles.chCommand->notify();
    delay(2000);
    ESP.restart();
  } else if (cmd == CMD_SET_CAP) {
    float v = _pendingParam;
    _pendingParam = NAN;
    if (isfinite(v) && v > 0.0f) {
      DBG_PRINTF("[BLE] Processing SET_CAP (main loop): %.3f Ah\n", v);
      // Delegate persistence to BatteryConfig via setBatteryCapacityAh()
      setBatteryCapacityAh(v);
      // Immediately update capacity characteristic so BLE clients see the new
      // value
      if (_handles.chCapacity) {
        char bufCap[32];
        snprintf(bufCap, sizeof(bufCap), "%.2f Ah", batteryCapacityAh);
        _handles.chCapacity->setValue(bufCap);
        _handles.chCapacity->notify();
      }
      // Publish capacity change over MQTT if connected
      if (mqtt.connected()) {
        char js[128];
        snprintf(js, sizeof(js), "{\"event\":\"cap_set\",\"value_Ah\":%.3f}",
                 v);
        mqtt.publish(MQTT_TOPIC, js, true);
        mqtt.loop();
      }
      // Use runtime value (already updated and persisted by
      // setBatteryCapacityAh)
      char buf[64];
      snprintf(buf, sizeof(buf), "CAP_SET:%.3fAh RB:%.3fAh", v,
               batteryCapacityAh);
      _handles.chCommand->setValue(buf);
      _handles.chCommand->notify();
    } else {
      _handles.chCommand->setValue("CAP_BAD_PARAM");
      _handles.chCommand->notify();
    }
  } else if (cmd == CMD_SET_BASE) {
    float v = _pendingParam;
    _pendingParam = NAN;
    if (isfinite(v) && v > 0.0f) {
      DBG_PRINTF("[BLE] Processing SET_BASE (main loop): %.3f mOhm\n", v);
      // Update learner baseline
      learner.setBaseline(v);
      // Publish baseline change over MQTT if connected
      if (mqtt.connected()) {
        char js[128];
        snprintf(js, sizeof(js),
                 "{\"event\":\"baseline_set\",\"value_mOhm\":%.3f}", v);
        mqtt.publish(MQTT_TOPIC, js, true);
        mqtt.loop();
      }
      char buf[32];
      snprintf(buf, sizeof(buf), "BASE_SET:%.3fmOhm", v);
      _handles.chCommand->setValue(buf);
      _handles.chCommand->notify();
    } else {
      _handles.chCommand->setValue("BASE_BAD_PARAM");
      _handles.chCommand->notify();
    }
  } else if (cmd == CMD_NVS_TEST) {
    DBG_PRINTLN("[BLE] Processing NVS_TEST (main loop)...");
    Preferences prefs;
    prefs.begin("battmon", false);
    uint32_t probe = (uint32_t)millis();
    bool wrote = prefs.putUInt("nvs_probe", probe);
    prefs.end();

    uint32_t rb = 0;
    bool has = false;
    Preferences prefs2;
    prefs2.begin("battmon", false);
    has = prefs2.isKey("nvs_probe");
    if (has)
      rb = prefs2.getUInt("nvs_probe", 0);
    prefs2.end();

    char msg[128];
    snprintf(msg, sizeof(msg), "NVS_TEST: put=%d key=%d wrote=%lu read=%lu",
             wrote ? 1 : 0, has ? 1 : 0, (unsigned long)probe,
             (unsigned long)rb);
    ble_notify_status(msg);
    _handles.chCommand->setValue("NVS_TEST_DONE");
    _handles.chCommand->notify();
  }
}

void ble_notify_status(const char *msg) {
  if (!msg)
    return;
  if (!ble.handles().chStatus)
    return;
  ble.handles().chStatus->setValue(msg);
  ble.handles().chStatus->notify();
}
