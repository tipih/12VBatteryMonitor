
#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <cstring>

struct DebugPublisher {
  PubSubClient *client = nullptr;
  const char *topic = nullptr;
  bool enabled = true;
  uint32_t minIntervalMs = 300;
  uint32_t lastSent_stepDetected = 0;
  uint32_t lastSent_stepRejected = 0;
  uint32_t lastSent_rintComputed = 0;
  uint32_t lastSent_baselineUpdate = 0;

  inline bool ok() const { return enabled && client && topic; }
  void send(const char *json, const char *typeTag) {
    if (!ok())
      return;
    uint32_t now = millis();
    uint32_t *gate = nullptr;
    if (!strcmp(typeTag, "step_detected"))
      gate = &lastSent_stepDetected;
    else if (!strcmp(typeTag, "step_rejected"))
      gate = &lastSent_stepRejected;
    else if (!strcmp(typeTag, "rint_computed"))
      gate = &lastSent_rintComputed;
    else if (!strcmp(typeTag, "baseline_update"))
      gate = &lastSent_baselineUpdate;
    if (gate && (now - *gate) < minIntervalMs)
      return;
    if (client->connected()) {
      client->publish(topic, json, false);
      if (gate)
        *gate = now;
    }
  }
};
