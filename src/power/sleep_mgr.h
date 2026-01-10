#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_sleep.h>

inline void goToDeepSleep(uint64_t interval_us) {
  NimBLEDevice::deinit(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  btStop();
  esp_sleep_enable_timer_wakeup(interval_us);
  esp_deep_sleep_start();
}
