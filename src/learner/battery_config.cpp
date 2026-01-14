#include "battery_config.h"
#include "../app_config.h"
#include "../comms/ble_mgr.h"
#include <Preferences.h>

BatteryConfig batteryConfig;

void BatteryConfig::begin() {
  Preferences prefs;
  prefs.begin("battmon", false);
  if (prefs.isKey("bat_cap")) {
    float v = prefs.getFloat("bat_cap", BATTERY_CAPACITY_AH);
    if (isfinite(v) && v > 0.0f)
      batteryCapacityAh = v;
  } else if (prefs.isKey("bat_cap2")) {
    float v = prefs.getFloat("bat_cap2", BATTERY_CAPACITY_AH);
    if (isfinite(v) && v > 0.0f)
      batteryCapacityAh = v;
  }
  prefs.end();
}

void BatteryConfig::setCapacity(float ah) {
  if (!isfinite(ah) || ah <= 0.0f)
    return;
  batteryCapacityAh = ah;

  // Try primary then fallback key with retries
  const char *primaryKey = "bat_cap";
  const char *fallbackKey = "bat_cap2";
  bool ok = false;
  bool usedFallback = false;
  for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
    Preferences prefs;
    prefs.begin("battmon", false);
    ok = prefs.putFloat(primaryKey, batteryCapacityAh);
    prefs.end();
    if (!ok)
      delay(50);
  }
  if (!ok) {
    for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
      Preferences prefs;
      prefs.begin("battmon", false);
      ok = prefs.putFloat(fallbackKey, batteryCapacityAh);
      prefs.end();
      if (ok)
        usedFallback = true;
      else
        delay(50);
    }
  }

  // verify readback
  float rb = NAN;
  bool hasP = false, hasF = false;
  Preferences prefsR;
  prefsR.begin("battmon", false);
  hasP = prefsR.isKey(primaryKey);
  hasF = prefsR.isKey(fallbackKey);
  if (hasP)
    rb = prefsR.getFloat(primaryKey, NAN);
  else if (hasF)
    rb = prefsR.getFloat(fallbackKey, NAN);
  prefsR.end();

  char msg[128];
  if (ok && (hasP || hasF) && isfinite(rb)) {
    snprintf(msg, sizeof(msg), "NVS: put=1 fb=%d P=%d F=%d CAP=%.3f RB=%.3f",
             usedFallback ? 1 : 0, hasP ? 1 : 0, hasF ? 1 : 0,
             batteryCapacityAh, rb);
  } else {
    snprintf(msg, sizeof(msg), "NVS: put=0 fb=%d P=%d F=%d CAP=%.3f RB=NONE",
             usedFallback ? 1 : 0, hasP ? 1 : 0, hasF ? 1 : 0,
             batteryCapacityAh);
  }
  ble_notify_status(msg);
}
