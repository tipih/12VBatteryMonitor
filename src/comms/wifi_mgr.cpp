#include "wifi_mgr.h"

bool WiFiMgr::connectSmart(const char *ssid, const char *pass,
                           uint32_t cooldownMs, uint8_t maxAttempts) {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  uint32_t now = millis();
  if (now - _lastAttemptMs < cooldownMs) {
    Serial.println("WiFi cooldown, skip.");
    return false;
  }

  // Ensure WiFi is fully off before starting
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint8_t attempts = 0;
#if debugWiFiMgr
  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(ssid);
  Serial.print("Connecting to WiFi with password:");
  Serial.println(pass);
#endif

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
#if debugWiFiMgr
    Serial.print(".");
#endif
    delay(500);
    attempts++;
#if debugWiFiMgr
    Serial.print("WiFi attempt ");
    Serial.println(attempts);
#endif
  }

  _lastAttemptMs = now;

  if (WiFi.status() == WL_CONNECTED) {
#if debugWiFiMgr
    Serial.println("");
    Serial.print("WiFi connected, IP address: ");
    Serial.println(WiFi.localIP());
#endif
    WiFi.setSleep(true);
    return true;
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return false;
}