
#pragma once
#include <Arduino.h>
#include <WiFi.h>
//#define debugWiFiMgr 1

class WiFiMgr {
public:
  bool connectSmart(const char* ssid, const char* pass, uint32_t cooldownMs, uint8_t maxAttempts);
  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  void powerSaveOn() { WiFi.setSleep(true); }
  void off() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
private:
  uint32_t _lastAttemptMs{ 0 };
};
