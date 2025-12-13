
#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#define debugMqttMgr 0

class MqttMgr {
public:
  explicit MqttMgr(Client& net) : _client(net) {}
  void setServer(const char* host, uint16_t port);
  bool ensureConnected(const char* clientId, const char* user, const char* pass, uint32_t timeoutMs = 7000);
  bool connected() { return _client.connected(); }
  bool publish(const char* topic, const char* payload, bool retain);
  void loop() { _client.loop(); }
  PubSubClient* raw() { return &_client; }

private:
  PubSubClient _client;
};