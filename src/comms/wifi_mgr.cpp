#include "wifi_mgr.h"

bool WiFiMgr::connectSmart(const char* ssid, const char* pass, uint32_t cooldownMs, uint8_t maxAttempts) {
  if (WiFi.status() == WL_CONNECTED) return true;
  uint32_t now = millis();
  if (now - _lastAttemptMs < cooldownMs) { Serial.println("WiFi cooldown, skip."); return false; }
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, pass);
  uint8_t attempts=0; 
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) { delay(500); attempts++; }
  
  _lastAttemptMs = now;
  
  if (WiFi.status() == WL_CONNECTED) { 
    WiFi.setSleep(true); 
    return true; 
}
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF); 
  return false;
}