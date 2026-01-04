#pragma once
#include <Arduino.h>
#include <secret.h>
// ------------------------------ USER CONFIG ------------------------------

// MQTT configuration (declared here, defined in app_config.cpp to avoid multiple definitions)
extern const char* MQTT_HOST;
extern const uint16_t MQTT_PORT;
extern const char* MQTT_CLIENT_ID;
extern const char* MQTT_TOPIC;
extern const char* MQTT_DBG_TOPIC;

const float BATTERY_CAPACITY_AH = 90.0f; //9.0f;    // LTX9-4 motorcycle battery //REMEMBER TO UPDATE

// Runtime-overridable battery capacity (Ah). Initialize with compile-time default
extern float batteryCapacityAh;
// Load persisted battery capacity from NVM (call from `setup()`)
void loadBatteryCapacityFromPrefs();
// Update runtime battery capacity and persist to NVM
void setBatteryCapacityAh(float ah);
const float INITIAL_BASELINE_mOHM = 35.0f;    // known-good baseline for LTX9-4

// Rest detection for OCV correction
const float    REST_CURRENT_THRESH_A = 0.60f;     // <= this considered "rest" (raised slightly)
const uint32_t REST_DETECT_SEC = 5 * 60;   // need 5 min rest for OCV snap
// Grace window for transient spikes: do not immediately clear `rest_accum_s`
const uint32_t REST_RESET_GRACE_SEC = 5;  // seconds of sustained non-rest before clearing rest_accum_s

// Hall (HSTS016L)
constexpr int   PIN_VOUT = 34;
constexpr int   PIN_VREF = 35;
constexpr bool  HAVE_VREF_PIN = true;
constexpr int   ADC_BITS = 12;
constexpr auto  ADC_ATTEN = ADC_11db;
constexpr float SENSOR_RATING_A = 130.0f; //200.0f;
constexpr int   HALL_SIGN = +1; // + discharge

// DS18B20
constexpr int      ONE_WIRE_PIN = 25;
constexpr uint8_t  DS_RES_BITS = 9;

// INA226 I2C
const uint8_t INA226_ADDR = 0x40; // default

// Alternator/DC-DC detection voltage
const float ALT_ON_VOLTAGE_V = 13.2f;

// Cadence (ACTIVE mode)
const uint32_t SAMPLE_INTERVAL_MS = 500;  // ~25 Hz V/I sampling
const uint32_t TEMP_INTERVAL_MS = 1000; // 1 Hz temperature sampling
const uint32_t PUBLISH_INTERVAL_MS = 2000; // every 2 s publish
const uint32_t SAMPLE_INTERVAL_MS_IDLE = 1000; // Reduced cadence while Parked&Idle but awake (save power)
const uint32_t PUBLISH_INTERVAL_MS_IDLE = 10000;// Reduced cadence while Parked&Idle but awake (save power)

// Temp compensation for Rint
const float REF_TEMP_C = 25.0f;
const float TEMP_ALPHA_PER_C = 0.0030f; // ≈0.3%/°C

// ------------------ Parked/Idle detection & sleep policy ------------------
const float    BASE_CONS_THRESH_A = 0.65f; // quiescent I threshold (parked/idle) — raised to reduce false positives
const uint32_t PARKED_IDLE_ENTRY_DWELL_SEC = 5 * 60;  // need 5 min quiet to enter Parked&Idle
const float    STEP_ACTIVITY_DI_A = 1.5f;    // ΔI>=1.5A counts as activity
const uint32_t STEP_ACTIVITY_WINDOW_MS = 200;     // within this time window
// 1 hour timer once Parked&Idle begins
const uint64_t PARKED_IDLE_MAX_MS = 10ULL * 60ULL * 1000ULL; // 20 min awake/idle before deep sleep
// Deep sleep cadence while parked
const uint64_t PARKED_WAKE_INTERVAL_US = 5ULL * 60ULL * 1000000ULL; // 5 min deep sleep

//BLE
static const char* BLE_DEVICE_NAME = "ESP32-BattMon";