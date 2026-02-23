
#pragma once
#include <Arduino.h>

struct TelemetryFrame {
  const char *mode; // "active" | "parked-idle" | "parked-sleep"
  float V, I, T;
  float soc_pct, soh_pct;
  float Rint_mOhm, Rint25_mOhm, RintBaseline_mOhm;
  float ah_left;
  float battery_capacity_ah;
  bool alternator_on;
  uint32_t rest_s, lowCurrentAccum_s, up_ms;
  bool hasRint, hasRint25;
};

bool buildTelemetryJson(const TelemetryFrame &f, char *out, size_t outLen);
