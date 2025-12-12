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

const float BATTERY_CAPACITY_AH   = 70.0f;    // adjust to your battery
const float INITIAL_BASELINE_mOHM = 10.0f;    // known-good baseline

// Rest detection for OCV correction
const float    REST_CURRENT_THRESH_A = 0.2f;     // <= this considered "rest"
const uint32_t REST_DETECT_SEC       = 5 * 60;   // need 5 min rest for OCV snap

// Hall (HSTS016L)
constexpr int   PIN_VOUT = 34;
constexpr int   PIN_VREF = 35;
constexpr bool  HAVE_VREF_PIN = true;
constexpr int   ADC_BITS = 12;
constexpr auto  ADC_ATTEN = ADC_11db;
constexpr float SENSOR_RATING_A = 120.0f; //200.0f;
constexpr int   HALL_SIGN = +1; // + discharge

// DS18B20
constexpr int      ONE_WIRE_PIN = 25;
constexpr uint8_t  DS_RES_BITS  = 9;

// INA226 I2C
const uint8_t INA226_ADDR = 0x40; // default

// Alternator/DC-DC detection voltage
const float ALT_ON_VOLTAGE_V = 13.2f;

// Cadence (ACTIVE mode)
const uint32_t SAMPLE_INTERVAL_MS       = 500;  // ~25 Hz V/I sampling
const uint32_t TEMP_INTERVAL_MS         = 1000; // 1 Hz temperature sampling
const uint32_t PUBLISH_INTERVAL_MS      = 2000; // every 2 s publish
const uint32_t SAMPLE_INTERVAL_MS_IDLE  = 1000; // Reduced cadence while Parked&Idle but awake (save power)
const uint32_t PUBLISH_INTERVAL_MS_IDLE = 10000;// Reduced cadence while Parked&Idle but awake (save power)

// Temp compensation for Rint
const float REF_TEMP_C       = 25.0f;
const float TEMP_ALPHA_PER_C = 0.0030f; // ≈0.3%/°C

// ------------------ Parked/Idle detection & sleep policy ------------------
const float    BASE_CONS_THRESH_A           = 0.25f; // quiescent I threshold (parked/idle)
const uint32_t PARKED_IDLE_ENTRY_DWELL_SEC  = 20 * 60; // need 20 min quiet to enter Parked&Idle
const float    STEP_ACTIVITY_DI_A           = 1.5f;    // ΔI>=1.5A counts as activity
const uint32_t STEP_ACTIVITY_WINDOW_MS      = 200;     // within this time window
// 5-hour timer once Parked&Idle begins
const uint32_t PARKED_IDLE_MAX_MS           = 5UL * 60UL * 60UL * 1000UL; // 5 h
// Deep sleep cadence while parked
const uint64_t PARKED_WAKE_INTERVAL_US      = 10ULL * 60ULL * 1000000ULL; // 10 min

//BLE
static const char* BLE_DEVICE_NAME = "ESP32-BattMon";