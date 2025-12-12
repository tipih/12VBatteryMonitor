#include "app_config.h"

// MQTT configuration definitions (single definition to avoid linker errors)
const char* MQTT_HOST = "192.168.0.54";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32-batt-hybrid-5h-debug-1";
const char* MQTT_TOPIC = "car/battery/telemetry";
const char* MQTT_DBG_TOPIC = "car/battery/debug/rint";
