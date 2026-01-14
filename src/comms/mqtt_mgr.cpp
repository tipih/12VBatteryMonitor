
#include "mqtt_mgr.h"

void MqttMgr::setServer(const char *host, uint16_t port) {
  _client.setServer(host, port);
}

bool MqttMgr::ensureConnected(const char *clientId, const char *user,
                              const char *pass, uint32_t timeoutMs) {
  uint32_t start = millis();
  while (!_client.connected() && millis() - start < timeoutMs) {
    if (_client.connect(clientId, user, pass)) {
#if debugMqttMgr
      Serial.println("MQTT connected.");
#endif
      return true;
      delay(200);
    }
  }
  Serial.println("MQTT connect timeout.");
  return _client.connected();
}

bool MqttMgr::publish(const char *topic, const char *payload, bool retain) {
#if debugMqttMgr
  Serial.print("Publishing to topic: ");
  Serial.println(topic);
  Serial.print("Payload length: ");
  Serial.println(strlen(payload));
  Serial.print("Payload: ");
  Serial.println(payload);
  Serial.print("MQTT connected: ");
  Serial.println(_client.connected() ? "YES" : "NO");
#endif

  if (!_client.connected()) {
#if debugMqttMgr
    Serial.println("ERROR: MQTT not connected, cannot publish.");
#endif
    return false;
  }

  bool result = _client.publish(topic, payload, retain);
#if debugMqttMgr
  Serial.print("Publish result: ");
  Serial.println(result ? "SUCCESS" : "FAILED");
#endif

  return result;
}
