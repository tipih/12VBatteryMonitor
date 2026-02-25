// ===== ESP32 12V Battery Monitor — Active ↔ Parked/Idle (5h) ↔ Deep-Sleep (10
// min) ===== Sensors: INA226 (I2C bus voltage), HSTS016L Hall (analog current),
// DS18B20 (temp) Telemetry: MQTT (JSON)
//
// File: main.cpp (Arduino/ESP32)

#include <ArduinoOTA.h>
#include <DallasTemperature.h>
#include <NimBLEDevice.h>
#include <OneWire.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <algorithm> // for std::sort (hall zero trimmed mean)
#include <app_config.h>
#include <battery/ocv_estimator.h>
#include <battery/state_detector.h>
#include <cmath>
#include <comms/ble_mgr.h>
#include <comms/debug_publisher.h>
#include <comms/mqtt_mgr.h>
#include <comms/wifi_mgr.h>
#include <cstring> // for strncmp, atoi
#include <esp_bt.h>
#include <esp_sleep.h>
#include <learner/rint_learner.h>
#include <power/sleep_mgr.h>
#include <sensor/ds18b20.h>
#include <sensor/hall_sensor.h>
#include <sensor/ina226.h>
#include <telemetry_payload.h>

// Forward-declare global mqtt instance (defined later in this file)
extern MqttMgr mqtt;

// Publish Home Assistant MQTT discovery config for sensors
void publishHADiscovery() {
  if (!mqtt.connected())
    return;
  const char *base = MQTT_TOPIC; // state topic
  const char *haId =
      "Toyota_batt_sensor"; // Home Assistant discovery ID / unique_id base
  // Device object
  char deviceJson[256];
  snprintf(deviceJson, sizeof(deviceJson),
           "{\"identifiers\":[\"%s\",\"%s\"],\"name\":\"Battery Monitor\"}",
           haId, MQTT_CLIENT_ID);

  // Helper buffers
  char topic[160];
  char payload[512];

  // Voltage
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_voltage/config",
           haId);
  // Format voltage as a two-decimal float for Home Assistant display
  // (e.g. 12.00)
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "Voltage\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"V\","
           "\"device_class\":\"voltage\",\"value_template\":\"{{ '%%.2f' | "
           "format(value_json.voltage_V) "
           "}}\",\"unique_id\":\"%s_voltage\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Current
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_current/config",
           haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "Current\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"A\","
           "\"value_template\":\"{{ value_json.current_A "
           "}}\",\"unique_id\":\"%s_current\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // SOC
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_soc/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "SOC\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\","
           "\"device_class\":\"battery\",\"value_template\":\"{{ "
           "value_json.soc_pct }}\",\"unique_id\":\"%s_soc\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // SOH
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_soh/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "SOH\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\","
           "\"value_template\":\"{{ value_json.soh_pct "
           "}}\",\"unique_id\":\"%s_soh\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Ah left (remaining capacity in Ah)
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_ah_left/config",
           haId);
  // Format Ah as a two-decimal float for Home Assistant display (e.g. 12.34)
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery Ah "
           "Left\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"Ah\","
           "\"value_template\":\"{{ '%%.2f' | format(value_json.ah_left) "
           "}}\",\"unique_id\":\"%s_ah_left\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Rint
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_rint/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Rint "
           "(mOhm)\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"mΩ\","
           "\"value_template\":\"{%% if value_json.Rint_mOhm is not none %%}{{ "
           "value_json.Rint_mOhm }}{%% else %%}{{ value_json.RintBaseline_mOhm "
           "}}{%% endif %%}\",\"unique_id\":\"%s_rint\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Alternator (binary sensor)
  snprintf(topic, sizeof(topic),
           "homeassistant/binary_sensor/%s_alternator/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Alternator "
           "On\",\"state_topic\":\"%s\",\"value_template\":\"{{ "
           "value_json.alternator_on "
           "}}\",\"payload_on\":\"true\",\"payload_off\":\"false\",\"unique_"
           "id\":\"%s_alternator\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Mode
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_mode/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "Mode\",\"state_topic\":\"%s\",\"value_template\":\"{{ "
           "value_json.mode }}\",\"unique_id\":\"%s_mode\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);

  // Uptime
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_uptime/config", haId);
  snprintf(payload, sizeof(payload),
           "{\"name\":\"Battery "
           "Uptime\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"s\","
           "\"value_template\":\"{{ (value_json.up_ms / 1000) | int "
           "}}\",\"unique_id\":\"%s_uptime\",\"device\":%s}",
           base, haId, deviceJson);
  mqtt.publish(topic, payload, true);
}

#define DEBUG_POWER_MANAGEMENT 1
// #define DEBUG_STATE_DETECTOR 1
// #define DEBUG_NO_SLEEP 1
#define DEBUG_PARKED_IDLE 1
// #define DEBUG_HALL_SENSOR 1

// Class init
DS18B20Sensor ds(ONE_WIRE_PIN, DS_RES_BITS);
INA226Bus ina(INA226_ADDR);
HallSensor hall(PIN_VOUT, PIN_VREF, HAVE_VREF_PIN, ADC_BITS, ADC_ATTEN,
                SENSOR_RATING_A, HALL_SIGN);
HallZeroStore hallZero;
BleMgr ble;
DebugPublisher gRintDbg;
WiFiMgr wifi;
WiFiClient _net;
MqttMgr mqtt(_net);
RintLearner learner;
BatteryStateDetector stateDetector;
OcvEstimator ocvEst; // Static helper class

// ==================== Zeroing =====================
// static float zero_mV = 0.0f;   // stored offset (Δ at zero current)

// ------------------------------ Globals ------------------------------

float soc_pct = 50.0f;
float rest_accum_s = 0.0f;
float rest_reset_accum_s =
    0.0f; // accumulate brief non-rest samples before clearing rest_accum_s
float last_I_A = 0.0f;
float last_V_V = 12.6f;
float last_T_C = 25.0f;
uint32_t lastSampleMs = 0;
uint32_t lastTempMs = 0;
uint32_t lastPublishMs = 0;
float lowCurrentAccum_s = 0.0f; // time I below threshold with alternator off
uint32_t parkedIdleEnterMs = 0; // when we entered Parked&Idle

enum Mode { MODE_ACTIVE = 0, MODE_PARKED_IDLE = 1 };
Mode mode = MODE_ACTIVE;

// OTA initialization guard
static bool otaInitialized = false;

// Complementary SOC filter params
static constexpr float SOC_FILTER_TAU_S =
    300.0f; // seconds (time constant toward OCV)
static constexpr float SOC_MIN_ALPHA = 0.02f; // minimum weight for coulomb SOC

// ------------------------------ OCV → SOC table at 25C
// ------------------------------
float socFromOCV_25C(float v) {
  struct {
    float v;
    float soc;
  } tbl[] = {{11.90f, 5.0f},  {12.00f, 15.0f}, {12.20f, 50.0f},
             {12.40f, 75.0f}, {12.60f, 95.0f}, {12.72f, 100.0f}};
  if (v <= tbl[0].v)
    return tbl[0].soc;
  for (int i = 1; i < (int)(sizeof(tbl) / sizeof(tbl[0])); ++i) {
    if (v <= tbl[i].v) {
      float t = (v - tbl[i - 1].v) / (tbl[i].v - tbl[i - 1].v);
      return tbl[i - 1].soc + t * (tbl[i].soc - tbl[i - 1].soc);
    }
  }
  return 100.0f;
}

float compensateOCVTo25C(float vbatt, float tempC) {
  if (!isfinite(tempC))
    return vbatt;
  float dv = -0.018f * (tempC - 25.0f); // ~-18 mV/°C for 12V
  return vbatt + dv;
}

// Note: alternator/step-activity detection is implemented in
// `BatteryStateDetector` (stateDetector) — use its methods.

// ------------------------------ Setup ------------------------------
void setup() {
  Serial.begin(115200);
  delay(30);
  Serial.println("System started");
  delay(1000);
  Wire.begin();
  Serial.println("Wire started");
  ds.begin();
  ina.begin();
  hall.begin();
  Serial.println("Dallas Temp started");

  // --- Hall zero on first boot: apply immediately ---
  if (hallZero.load()) {
    hall.setZero(hallZero.zero_mV);
    Serial.printf("Loaded HALL zero: %.3f mV\n", hallZero.zero_mV);
  } else {
    float z = hall.captureZeroTrimmedMean(64);
    hall.setZero(z);  // <-- apply now
    hallZero.save(z); // persist
  }

  // Temperature seed
  last_T_C = ds.readTempC();
  Serial.print("Dallas Read Temp ");
  Serial.println(last_T_C);

  // BLE
  ble.begin(BLE_DEVICE_NAME);

  // If woke from timer and still idle → snapshot-only & back to sleep
  bool snapshotOnly = false;
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  bool wokeFromTimer = (cause == ESP_SLEEP_WAKEUP_TIMER);
  Serial.print("Wakeup cause: ");
  Serial.println(cause);

  if (wokeFromTimer) {
    float V0 = ina.readBusVoltage_V();
    float I0 = hall.readCurrentA(16);
    if (isfinite(V0))
      last_V_V = V0;
    last_I_A = I0;
    bool altOn = stateDetector.alternatorOn(last_V_V);
    bool activeNow = altOn || (fabsf(last_I_A) > BASE_CONS_THRESH_A);
    snapshotOnly = !activeNow;
  }

  // Wi-Fi & MQTT (lwIP initialized after 500ms delay)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  delay(500); // Give lwIP time to initialize

  // Try WiFi connection in setup (will retry in loop if needed)
  // Use 0 cooldown for snapshot mode (deep sleep wakeup) to ensure connection
  uint32_t wifiCooldown = snapshotOnly ? 0 : 10000;
  wifi.connectSmart(WIFI_SSID, WIFI_PASSWORD, wifiCooldown, /*attempts*/ 15);
  if (wifi.connected()) {
    mqtt.ensureConnected(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
    // Initialize OTA if Wi-Fi is available. If `OTA_PASSWORD` macro is defined
    // in your `src/secret.h`, it will be applied; otherwise OTA will run
    // without a password.
    ArduinoOTA.setHostname(MQTT_CLIENT_ID);
#ifdef OTA_PASSWORD
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
    ArduinoOTA.onStart([]() { Serial.println("OTA Start TEST"); });
    ArduinoOTA.onEnd([]() { Serial.println("OTA End"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t err) {
      Serial.printf("OTA Error[%u]: ", (unsigned)err);
      if (err == OTA_AUTH_ERROR)
        Serial.println("Auth Failed");
      else if (err == OTA_BEGIN_ERROR)
        Serial.println("Begin Failed");
      else if (err == OTA_CONNECT_ERROR)
        Serial.println("Connect Failed");
      else if (err == OTA_RECEIVE_ERROR)
        Serial.println("Receive Failed");
      else if (err == OTA_END_ERROR)
        Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    // Publish device IP as retained message to <MQTT_TOPIC>/ip
    {
      IPAddress ip = WiFi.localIP();
      char ipStr[32];
      snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      char ipTopic[80];
      snprintf(ipTopic, sizeof(ipTopic), "%s/ip", MQTT_TOPIC);
      if (mqtt.connected()) {
        mqtt.publish(ipTopic, ipStr, true);
        mqtt.loop();
        delay(50);
        // Publish Home Assistant discovery configs
        publishHADiscovery();
      }
    }
  }

  // Wire debug publisher
  gRintDbg.client = mqtt.raw();
  gRintDbg.topic = MQTT_DBG_TOPIC;
  gRintDbg.enabled = true;                         // set false to silence
  gRintDbg.minIntervalMs = 250;                    // per-event rate limit
  learner.begin(INITIAL_BASELINE_mOHM, &gRintDbg); // enable when needed
  // Load persisted battery capacity (if previously set via BLE)
  loadBatteryCapacityFromPrefs();

  // Load SOC from NVM (battmon namespace already opened by learner)
  Preferences prefs;
  prefs.begin("battmon", false);
  soc_pct = prefs.getFloat("soc_pct", 90.0f);
  if (!isfinite(soc_pct) || soc_pct < 0.0f || soc_pct > 100.0f) {
    soc_pct = 90.0f;
  }
  prefs.end();

  uint32_t now = millis();
  lastSampleMs = now;
  lastTempMs = now;
  lastPublishMs = now;
  lowCurrentAccum_s = 0.0f;
  mode = MODE_ACTIVE;

  stateDetector.reset();

  if (snapshotOnly) {
    float rint_mOhm = learner.lastRint_mOhm();
    float rint25_mOhm = learner.lastRint25_mOhm();
    float base_mOhm = learner.baseline_mOhm();
    float soh = learner.currentSOH();
    // Filter extreme Rint values (starter/transient artifacts)
    float rint_mOhm_f =
        (isfinite(rint_mOhm) && rint_mOhm <= RINT_MAX_VALID_MOHM) ? rint_mOhm
                                                                  : base_mOhm;
    float rint25_mOhm_f =
        (isfinite(rint25_mOhm) && rint25_mOhm <= RINT_MAX_VALID_MOHM)
            ? rint25_mOhm
            : base_mOhm;

    // Blend SOC with OCV (snapshot uses adaptive complementary filter too)
    float soc_for_snapshot = soc_pct;
    if (isfinite(last_V_V) && isfinite(last_T_C)) {
      float v25 = compensateOCVTo25C(last_V_V, last_T_C);
      float ocvSOC = socFromOCV_25C(v25);
      float alpha = SOC_MIN_ALPHA + (1.0f - SOC_MIN_ALPHA) *
                                        expf(-rest_accum_s / SOC_FILTER_TAU_S);
      soc_for_snapshot = alpha * soc_pct + (1.0f - alpha) * ocvSOC;
    }
    // Estimate Ah left based on rated capacity, SOH and blended SOC
    float soh_frac = soh;
    if (soh_frac > 1.1f)
      soh_frac /= 100.0f;                       // guard against percent return
    float C_eff = batteryCapacityAh * soh_frac; // effective usable Ah
    float ah_left_snapshot = C_eff * (soc_for_snapshot / 100.0f);

    TelemetryFrame tf{
        .mode = "snapshot",
        .V = last_V_V,
        .I = last_I_A,
        .T = last_T_C,
        .soc_pct = soc_for_snapshot,
        .soh_pct = soh * 100.0f,
        .Rint_mOhm = rint_mOhm_f,
        .Rint25_mOhm = rint25_mOhm_f,
        .RintBaseline_mOhm = base_mOhm,
        .ah_left = ah_left_snapshot,
        .alternator_on = stateDetector.alternatorOn(last_V_V),
        .rest_s = (uint32_t)rest_accum_s,
        .lowCurrentAccum_s = (uint32_t)lowCurrentAccum_s,
        .up_ms = millis(),
        .hasRint = (isfinite(rint_mOhm) && rint_mOhm <= RINT_MAX_VALID_MOHM),
        .hasRint25 =
            (isfinite(rint25_mOhm) && rint25_mOhm <= RINT_MAX_VALID_MOHM)};
    Serial.println("Publishing snapshot telemetry");
    Serial.printf(
        "V: %.3f V, I: %.3f A, T: %.1f C, SOC: %.1f %%, SOH: %.1f %%\n", tf.V,
        tf.I, tf.T, tf.soc_pct, tf.soh_pct);

    // Update BLE characteristics (include Ah-left)
    ble.update(tf.V, tf.I, tf.T, "snapshot", tf.soc_pct, tf.soh_pct,
               tf.ah_left);
    ble.process();

    char payload[700];
    if (buildTelemetryJson(tf, payload, sizeof(payload))) {
      if (wifi.connected() && mqtt.connected()) {
        mqtt.publish(MQTT_TOPIC, payload, true);
        mqtt.loop();
        delay(100); // Give time for message to send
        Serial.println("MQTT message sent successfully");
      } else {
        Serial.println("WARNING: WiFi/MQTT not connected, message not sent");
      }
    }

    // Properly disconnect WiFi before sleep
    wifi.off();
    delay(100);
    Serial.println("go to sleep");

#ifndef DEBUG_NO_SLEEP
    goToDeepSleep(PARKED_WAKE_INTERVAL_US);
#endif
  }
}

// (removed duplicate HA discovery block — publishHADiscovery() already
// publishes Ah_left)
// ------------------------------ Loop ------------------------------
void loop() {

  uint32_t now = millis();

  // Cadence by mode
  uint32_t sampleInterval =
      (mode == MODE_ACTIVE) ? SAMPLE_INTERVAL_MS : SAMPLE_INTERVAL_MS_IDLE;
  uint32_t publishInterval =
      (mode == MODE_ACTIVE) ? PUBLISH_INTERVAL_MS : PUBLISH_INTERVAL_MS_IDLE;

  // --- V/I sampling ---
  if (now - lastSampleMs >= sampleInterval) {
    float V = ina.readBusVoltage_V();
    //    Serial.print("Voltage ");
    //    Serial.println(V);
    float I = hall.readCurrentA(64); // use 64 samples for better stability

    // Coulomb counting
    if (batteryCapacityAh > 0) {
      float dt_s = (now - lastSampleMs) / 1000.0f;
      float I_avg = 0.5f * (last_I_A + I);
      float dAh = I_avg * (dt_s / 3600.0f);
      float oldSOC = soc_pct;
      soc_pct -= (dAh / batteryCapacityAh) * 100.0f;
      // Save to NVM if SOC changed significantly (avoid excessive writes)
      if (fabsf(soc_pct - oldSOC) > 0.5f) {
        Preferences prefs;
        prefs.begin("battmon", false);
        prefs.putFloat("soc_pct", soc_pct);
        prefs.end();
      }
    }

    // Rest accumulation for OCV correction

    float dt_s = (now - lastSampleMs) / 1000.0f;
    if (fabsf(I) < REST_CURRENT_THRESH_A && !stateDetector.alternatorOn(V)) {
      rest_accum_s += dt_s;
      rest_reset_accum_s = 0.0f;
    } else {
      // brief spikes should not immediately clear the rest accumulator — only
      // clear if non-rest condition persists for REST_RESET_GRACE_SEC seconds
      rest_reset_accum_s += dt_s;
      if (rest_reset_accum_s >= (float)REST_RESET_GRACE_SEC) {
        rest_accum_s = 0.0f;
        rest_reset_accum_s = 0.0f;
      }
    }

    // Parked/Idle detection
    bool altOn = stateDetector.alternatorOn(V);
    bool activity = stateDetector.hasRecentActivity(I, now);

#ifdef DEBUG_STATE_DETECTOR
    Serial.print("StateDetector: ");
    Serial.print("altOn=");
    Serial.print(altOn ? "true" : "false");
    Serial.print(", activity=");
    Serial.println(activity ? "true" : "false");

    Serial.print("!altOn && fabsf(I):  ");
    Serial.print((!altOn && fabsf(I)));
#endif
    if (!altOn && fabsf(I) < BASE_CONS_THRESH_A && !activity) {
      lowCurrentAccum_s += (now - lastSampleMs) / 1000.0f;
#ifdef DEBUG_STATE_DETECTOR
      Serial.print(":  lowCurrentAccum_s=");
      Serial.print(lowCurrentAccum_s);
      Serial.print(": Mode ");
      Serial.println((mode == MODE_ACTIVE) ? "ACTIVE" : "PARKED-IDLE");
#endif
    } else {
      lowCurrentAccum_s = 0.0f;
      if (mode == MODE_PARKED_IDLE)
        mode = MODE_ACTIVE; // exit parked-idle on activity
    }

    // Enter Parked&Idle after dwell
    if (mode == MODE_ACTIVE &&
        lowCurrentAccum_s >= (float)PARKED_IDLE_ENTRY_DWELL_SEC) {
      mode = MODE_PARKED_IDLE;
      parkedIdleEnterMs = now;
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        mqtt.publish(MQTT_TOPIC, "{\"mode\":\"parked-idle\"}", true);
        mqtt.loop();
      }
    }

    // Clamp SOC
    if (!isfinite(soc_pct))
      soc_pct = 50.0f;
    soc_pct = fmaxf(0.0f, fminf(100.0f, soc_pct));

    // Update “last” values once per tick (canonical spot)
    if (isfinite(V))
      last_V_V = V;
    last_I_A = I;
    lastSampleMs = now;

    // Feed learner with the current sample (either V/I or last_*)
    if (fabsf(last_I_A) <= RINT_INGEST_MAX_I_A) {
      learner.ingest(last_V_V, last_I_A, (isfinite(last_T_C) ? last_T_C : NAN),
                     now);
    }

    // Debug: Show if Rint was calculated (ignore extreme spike values)
    static float prevRint = NAN;
    float currRint = learner.lastRint_mOhm();
    float currRint25 = learner.lastRint25_mOhm();
    if (isfinite(currRint) && currRint <= RINT_MAX_VALID_MOHM &&
        currRint != prevRint) {
      Serial.printf("*** NEW RINT CALCULATED: %.2f mOhm (25C: %.2f mOhm) ***\n",
                    currRint, currRint25);
      prevRint = currRint;
    }
  }

  // --- Temperature @1 Hz ---
  if (now - lastTempMs >= TEMP_INTERVAL_MS) {
    float tC = ds.readTempC();
    if (isfinite(tC))
      last_T_C = tC;

    bool altOff = !stateDetector.alternatorOn(last_V_V);
    if (altOff && rest_accum_s >= (float)REST_DETECT_SEC &&
        isfinite(last_V_V)) {
      float v25 = compensateOCVTo25C(last_V_V, last_T_C);
      float ocvSOC = socFromOCV_25C(v25);
      float oldSOC = soc_pct;
      // Adaptive complementary filter: weight coulomb SOC by alpha that
      // decreases as rest_accum_s grows (trust OCV more when rested).
      float alpha = SOC_MIN_ALPHA + (1.0f - SOC_MIN_ALPHA) *
                                        expf(-rest_accum_s / SOC_FILTER_TAU_S);
      float newSoc = alpha * soc_pct + (1.0f - alpha) * ocvSOC;
      soc_pct = fmaxf(0.0f, fminf(100.0f, newSoc));
      // Save to NVM after OCV correction if it moved significantly
      if (fabsf(soc_pct - oldSOC) > 0.5f) {
        Preferences prefs;
        prefs.begin("battmon", false);
        prefs.putFloat("soc_pct", soc_pct);
        prefs.end();
      }
    }
    lastTempMs = now;
  }

  // --- Publish after sampling ---

  if (now - lastPublishMs >= publishInterval) {

    if (!wifi.connected()) {
      // Serial.println("WiFi not connected, try connect");
      wifi.connectSmart(WIFI_SSID, WIFI_PASSWORD, 10000, 10);
    }
    if (wifi.connected() && !mqtt.connected()) {
      // Serial.println("MQTT not connected, try connect");
      mqtt.ensureConnected(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
      // Initialize OTA if not already initialized
      if (!otaInitialized) {
        ArduinoOTA.setHostname(MQTT_CLIENT_ID);
#ifdef OTA_PASSWORD
        ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
        ArduinoOTA.onStart([]() { Serial.println("OTA Start"); });
        ArduinoOTA.onEnd([]() { Serial.println("OTA End"); });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t err) {
          Serial.printf("OTA Error[%u]: ", (unsigned)err);
          if (err == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
          else if (err == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
          else if (err == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
          else if (err == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
          else if (err == OTA_END_ERROR)
            Serial.println("End Failed");
        });
        ArduinoOTA.begin();
        otaInitialized = true;
        Serial.println("ArduinoOTA initialized (on reconnect)");
      }
      // Publish/refresh device IP as retained message to <MQTT_TOPIC>/ip after
      // reconnect
      {
        IPAddress ip = WiFi.localIP();
        char ipStr[32];
        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2],
                 ip[3]);
        char ipTopic[80];
        snprintf(ipTopic, sizeof(ipTopic), "%s/ip", MQTT_TOPIC);
        if (mqtt.connected()) {
          mqtt.publish(ipTopic, ipStr, true);
          mqtt.loop();
          // Refresh Home Assistant discovery configs on reconnect
          publishHADiscovery();
        }
      }
    }

    // Serial.println("prepare to send");

    bool altOn = stateDetector.alternatorOn(last_V_V);
    float lastRint = learner.lastRint_mOhm();
    float lastRint25 = learner.lastRint25_mOhm();
    float baseR = learner.baseline_mOhm();
    float soh = learner.currentSOH();
    // Filter extreme Rint values (starter/transient artifacts)
    float lastRint_f = (isfinite(lastRint) && lastRint <= RINT_MAX_VALID_MOHM)
                           ? lastRint
                           : baseR;
    float lastRint25_f =
        (isfinite(lastRint25) && lastRint25 <= RINT_MAX_VALID_MOHM) ? lastRint25
                                                                    : baseR;

    // Blend SOC with OCV before Ah calculation
    float soc_for_publish = soc_pct;
    if (isfinite(last_V_V) && isfinite(last_T_C)) {
      float v25 = compensateOCVTo25C(last_V_V, last_T_C);
      float ocvSOC = socFromOCV_25C(v25);
      float alpha = SOC_MIN_ALPHA + (1.0f - SOC_MIN_ALPHA) *
                                        expf(-rest_accum_s / SOC_FILTER_TAU_S);
      soc_for_publish = alpha * soc_pct + (1.0f - alpha) * ocvSOC;
    }
    // Estimate Ah left based on rated capacity, SOH and blended SOC
    float soh_frac = soh;
    if (soh_frac > 1.1f)
      soh_frac /= 100.0f;
    float C_eff = batteryCapacityAh * soh_frac;
    float ah_left = C_eff * (soc_for_publish / 100.0f);

    Serial.print("Mode:  ");
    Serial.print((mode == MODE_ACTIVE) ? "ACTIVE" : "PARKED-IDLE");
    Serial.print(" %, SOH: ");
    Serial.print(soh * 100.0f);
    Serial.print(" %, SOC: ");
    Serial.print(soc_pct * 1.0f);
    Serial.printf(" Rint: %.2f mOhm, Rint25: %.2f mOhm, BaseR: %.2f mOhm\n",
                  lastRint_f, lastRint25_f, baseR);
    Serial.println();

    TelemetryFrame tf{
        .mode = (mode == MODE_ACTIVE ? "active" : "parked-idle"),
        .V = last_V_V,
        .I = last_I_A,
        .T = last_T_C,
        .soc_pct = soc_for_publish,
        .soh_pct = soh * 100.0f,
        .Rint_mOhm = lastRint_f,
        .Rint25_mOhm = lastRint25_f,
        .RintBaseline_mOhm = baseR,
        .ah_left = ah_left,
        .alternator_on = altOn,
        .rest_s = (uint32_t)rest_accum_s,
        .lowCurrentAccum_s = (uint32_t)lowCurrentAccum_s,
        .up_ms = now,
        .hasRint = (isfinite(lastRint) && lastRint <= RINT_MAX_VALID_MOHM),
        .hasRint25 =
            (isfinite(lastRint25) && lastRint25 <= RINT_MAX_VALID_MOHM)};

    char payload[700];
    if (buildTelemetryJson(tf, payload, sizeof(payload))) {
      // Serial.printf("Payload length: %u", (unsigned)strlen(payload));
      // Serial.println("");

      // Call loop() to keep connection alive before publish
      mqtt.loop();
      delay(10);

      mqtt.publish(MQTT_TOPIC, payload, false);
      mqtt.loop();
    }

    ble.update(last_V_V, last_I_A, last_T_C, tf.mode, soc_pct, soh * 100.0f,
               ah_left);
    ble.process();
#if DEBUG_POWER_MANAGEMENT
    Serial.print("Voltage:");
    Serial.print(last_V_V);
    Serial.print(", Current:");
    Serial.print(last_I_A);
    Serial.print(" Time (s): ");
    Serial.println((now - parkedIdleEnterMs) / 1000);
#endif

    if (mqtt.connected()) {
    }
    lastPublishMs = now;
  }

  // --- 5h timer: Parked&Idle → Deep Sleep (10 min cadence) ---
  if (mode == MODE_PARKED_IDLE) {
#if DEBUG_PARKED_IDLE
    static uint32_t lastParkedPrintMs = 0;
    if (now - lastParkedPrintMs >= 1000) {
      Serial.print("Parked&Idle time (s): ");
      Serial.print((now - parkedIdleEnterMs) / 1000);
      Serial.println();
      Serial.print("Time to deep sleep (s): ");
      Serial.print((PARKED_IDLE_MAX_MS - (now - parkedIdleEnterMs)) / 1000);
      Serial.println();
      lastParkedPrintMs = now;
    }
#endif

    // Ensure we respect a configured timeout but never sleep before
    // `MIN_PARKED_IDLE_BEFORE_SLEEP_MS` has elapsed after entering Parked&Idle.
    uint64_t effectiveTimeout =
        (PARKED_IDLE_MAX_MS >= MIN_PARKED_IDLE_BEFORE_SLEEP_MS)
            ? PARKED_IDLE_MAX_MS
            : MIN_PARKED_IDLE_BEFORE_SLEEP_MS;
    if ((now - parkedIdleEnterMs) >= effectiveTimeout) {
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "{\"mode\":\"parked-sleep\",\"sleep_s\":%lu}",
                 (unsigned long)(PARKED_WAKE_INTERVAL_US / 1000000ULL));
        mqtt.publish(MQTT_TOPIC, msg, true);
        mqtt.loop();
        delay(50);
      }
      goToDeepSleep(PARKED_WAKE_INTERVAL_US);
    }
  }

  ArduinoOTA.handle();
  mqtt.loop();
}
