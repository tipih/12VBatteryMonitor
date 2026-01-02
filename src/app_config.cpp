#include "app_config.h"

// MQTT configuration definitions (single definition to avoid linker errors)
const char* MQTT_HOST = "192.168.0.54";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32-batt-hybrid-5h-debug-1";
const char* MQTT_TOPIC = "car/battery/telemetry";
const char* MQTT_DBG_TOPIC = "car/battery/debug/rint";

#include <Preferences.h>

// Runtime battery capacity (Ah) - initialized from compile-time default
float batteryCapacityAh = BATTERY_CAPACITY_AH;

void loadBatteryCapacityFromPrefs() {
    Preferences prefs;
    prefs.begin("battmon", false);
    if (prefs.isKey("battery_capacity_Ah")) {
        float v = prefs.getFloat("battery_capacity_Ah", BATTERY_CAPACITY_AH);
        if (isfinite(v) && v > 0.0f) batteryCapacityAh = v;
    }
    prefs.end();
}

void setBatteryCapacityAh(float ah) {
    if (!isfinite(ah) || ah <= 0.0f) return;
    batteryCapacityAh = ah;
    Preferences prefs;
    prefs.begin("battmon", false);
    prefs.putFloat("battery_capacity_Ah", batteryCapacityAh);
    prefs.end();
}
