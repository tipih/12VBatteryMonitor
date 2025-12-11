
#include "mqtt_mgr.h"

void MqttMgr::setServer(const char* host, uint16_t port) { _client.setServer(host, port); }

bool MqttMgr::ensureConnected(const char* clientId, const char* user, const char* pass, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!_client.connected() && millis() - start < timeoutMs) {
    if (_client.connect(clientId, user, pass)) return true; delay(200);
  }
  return _client.connected();
}

bool MqttMgr::publish(const char* topic, const char* payload, bool retain) {
  if (!_client.connected()) return false; return _client.publish(topic, payload, retain);
}
