#include "app_config.h"

#include <comms/ble_mgr.h>

// MQTT configuration definitions (single definition to avoid linker errors)
const char* MQTT_HOST = "192.168.0.54";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32-batt-hybrid-5h-debug-1";
const char* MQTT_TOPIC = "car/battery/telemetry";
const char* MQTT_DBG_TOPIC = "car/battery/debug/rint";


#include <Preferences.h>
#include "learner/battery_config.h"

// Runtime battery capacity (Ah) - initialized from compile-time default
float batteryCapacityAh = BATTERY_CAPACITY_AH;

void loadBatteryCapacityFromPrefs() {
    // Delegate loading to the centralized BatteryConfig helper which
    // knows the correct NVS keys and verification logic.
    batteryConfig.begin();
}

void setBatteryCapacityAh(float ah) {
    if (!isfinite(ah) || ah <= 0.0f) return;
    batteryCapacityAh = ah;
    // Delegate persistence to BatteryConfig to keep NVS writes centralized.
    batteryConfig.setCapacity(ah);
}
