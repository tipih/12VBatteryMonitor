// ===== ESP32 12V Battery Monitor — Active ↔ Parked/Idle (5h) ↔ Deep-Sleep (10 min) =====
// Sensors: INA226 (I2C bus voltage), HSTS016L Hall (analog current), DS18B20 (temp)
// Telemetry: MQTT (JSON)
//
// File: main.cpp (Arduino/ESP32)

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_bt.h>
#include <algorithm>    // for std::sort (hall zero trimmed mean)
#include <cstring>      // for strncmp, atoi
#include <NimBLEDevice.h>
#include <app_config.h>
#include <power/sleep_mgr.h>
#include <sensor/ds18b20.h>
#include <sensor/ina226.h>
#include <sensor/hall_sensor.h>
#include <comms/ble_mgr.h>
#include <comms/debug_publisher.h>
#include <comms/mqtt_mgr.h>
#include <comms/wifi_mgr.h>
#include <telemetry_payload.h>
#include <learner/rint_learner.h>






//Class init
DS18B20Sensor   ds(ONE_WIRE_PIN, DS_RES_BITS);
INA226Bus       ina(INA226_ADDR);
HallSensor      hall(PIN_VOUT, PIN_VREF, HAVE_VREF_PIN, ADC_BITS, ADC_ATTEN, SENSOR_RATING_A, HALL_SIGN);
HallZeroStore   hallZero;
BleMgr          ble;
DebugPublisher  gRintDbg;
WiFiMgr         wifi;
WiFiClient      _net;
MqttMgr         mqtt(_net);
RintLearner     learner;
// ==================== Options =====================
static bool        MEASURE_RMS             = false; // false = DC/average, true = RMS (future use)
static uint32_t    SAMPLE_HZ               = 4000;  // RMS sample rate (future use)
static const uint32_t RMS_WINDOW_MS        = 1000;  // RMS window length ~1 s (future use)

// ==================== Zeroing =====================
static float zero_mV = 0.0f;   // stored offset (Δ at zero current)

// ==================== Utils =======================
static inline uint32_t us_now() { return micros(); }




// ------------------------------ Globals ------------------------------

Preferences        hallPrefs;  // dedicated NVS bucket for Hall calibration

float     soc_pct            = 90.0f;
float     rest_accum_s       = 0.0f;
float     last_I_A           = 0.0f;
float     last_V_V           = 12.6f;
float     last_T_C           = 25.0f;
uint32_t  lastSampleMs       = 0;
uint32_t  lastTempMs         = 0;
uint32_t  lastPublishMs      = 0;
float     lowCurrentAccum_s  = 0.0f; // time I below threshold with alternator off
uint32_t  parkedIdleEnterMs  = 0;    // when we entered Parked&Idle

enum Mode { MODE_ACTIVE=0, MODE_PARKED_IDLE=1 };
Mode mode = MODE_ACTIVE;


// ------------------------------ OCV → SOC table at 25C ------------------------------
float socFromOCV_25C(float v) {
  struct { float v; float soc; } tbl[] = {
    {11.90f, 5.0f}, {12.00f, 15.0f}, {12.20f, 50.0f},
    {12.40f, 75.0f}, {12.60f, 95.0f}, {12.72f,100.0f}
  };
  if (v <= tbl[0].v) return tbl[0].soc;
  for (int i=1; i<(int)(sizeof(tbl)/sizeof(tbl[0])); ++i) {
    if (v <= tbl[i].v) {
      float t = (v - tbl[i-1].v) / (tbl[i].v - tbl[i-1].v);
      return tbl[i-1].soc + t * (tbl[i].soc - tbl[i-1].soc);
    }
  }
  return 100.0f;
}

float compensateOCVTo25C(float vbatt, float tempC) {
  if (!isfinite(tempC)) return vbatt;
  float dv = -0.018f * (tempC - 25.0f); // ~-18 mV/°C for 12V
  return vbatt + dv;
}

bool alternatorOnByV(float V) { return (V >= ALT_ON_VOLTAGE_V); }

bool recentStepActivity(float currentNow, uint32_t nowMs) {
  static float prevI = NAN; static uint32_t prevMs = 0;
  bool active = false;
  if (isfinite(prevI)) {
    float dI = fabsf(currentNow - prevI);
    uint32_t dt = nowMs - prevMs;
    if (dI >= STEP_ACTIVITY_DI_A && dt <= STEP_ACTIVITY_WINDOW_MS) active = true;
  }
  prevI = currentNow; prevMs = nowMs;
  return active;
}




// ------------------------------ Setup ------------------------------
void setup() {
  Serial.begin(115200); delay(30);
  Serial.print("System started");
  delay(1000);
  Wire.begin();
  Serial.println("Wire started");
  ds.begin();
  ina.begin();
  hall.begin(); 
  
  Serial.println("Dallas Temp started");

  // Hall zero
  if (hallZero.load()) {
    hall.setZero(hallZero.zero_mV);
    Serial.printf("Loaded HALL zero: %.3f mV", hallZero.zero_mV);
  } else {
    float z = hall.captureZeroTrimmedMean(64);
    hallZero.save(z);
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
 
 
  if (wokeFromTimer) {
    float V0 = ina.readBusVoltage_V();
    float I0 = hall.readCurrentA(16);
    if (isfinite(V0)) last_V_V = V0;
    last_I_A = I0;
    bool altOn = alternatorOnByV(last_V_V);
    bool activeNow = altOn || (fabsf(last_I_A) > BASE_CONS_THRESH_A);
    snapshotOnly = !activeNow;

  }
  
  // Wi-Fi & MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  wifi.connectSmart(WIFI_SSID, WIFI_PASSWORD, /*cooldown*/10000, /*attempts*/5);
  mqtt.ensureConnected(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);

  // Wire debug publisher
  gRintDbg.client        = mqtt.raw();
  gRintDbg.topic         = MQTT_DBG_TOPIC;
  gRintDbg.enabled       = true;   // set false to silence
  gRintDbg.minIntervalMs = 250;    // per-event rate limit
  learner.begin(INITIAL_BASELINE_mOHM, &gRintDbg); // enable when needed

  uint32_t now = millis();
  lastSampleMs  = now;
  lastTempMs    = now;
  lastPublishMs = now;
  lowCurrentAccum_s = 0.0f;
  mode = MODE_ACTIVE;

  if (snapshotOnly) {
    float rint_mOhm   =  learner.lastRint_mOhm();
    float rint25_mOhm =  learner.lastRint25_mOhm();
    float base_mOhm   =  learner.baseline_mOhm();
    float soh         =  learner.currentSOH();



    if (mqtt.connected()) {
      Serial.println("Prepare to send");
      char payload[560];

      snprintf(payload, sizeof(payload),
          "{...,"                                   // your other fields
          "\"soh_pct\":%s,"                         // guard SOH when no R25
          "\"Rint_mOhm\":%s,"
          "\"Rint25_mOhm\":%s,"
          "\"RintBaseline_mOhm\":%.2f,"
          "...}",
          (isfinite(rint25_mOhm) ? String(100.0f * soh, 1).c_str() : "null"),
          (isfinite(rint_mOhm)   ? String(rint_mOhm, 2).c_str()     : "null"),
          (isfinite(rint25_mOhm) ? String(rint25_mOhm, 2).c_str()   : "null"),
          base_mOhm);

            mqtt.publish(MQTT_TOPIC, payload, true);
            mqtt.loop();
            delay(50);
          }
    Serial.println("go to sleep");
    //goToDeepSleep(PARKED_WAKE_INTERVAL_US); // keep disabled while debugging
  }
}

// ------------------------------ Loop ------------------------------
void loop() {


  uint32_t now = millis();

  // Cadence by mode
  uint32_t sampleInterval  = (mode == MODE_ACTIVE) ? SAMPLE_INTERVAL_MS : SAMPLE_INTERVAL_MS_IDLE;
  uint32_t publishInterval = (mode == MODE_ACTIVE) ? PUBLISH_INTERVAL_MS : PUBLISH_INTERVAL_MS_IDLE;

  // --- V/I sampling ---
  if (now - lastSampleMs >= sampleInterval) {
    float V = ina.readBusVoltage_V();
    Serial.print("Voltage ");
    Serial.println(V);
    float I = hall.readCurrentA(32);   // use 32 samples for stability



  
      

  // Coulomb counting
    if (BATTERY_CAPACITY_AH > 0) {
      float dt_s  = (now - lastSampleMs) / 1000.0f;
      float I_avg = 0.5f * (last_I_A + I);
      float dAh   = I_avg * (dt_s / 3600.0f);
      soc_pct    -= (dAh / BATTERY_CAPACITY_AH) * 100.0f;
    }

    // Rest accumulation for OCV correction

    if (fabsf(I) < REST_CURRENT_THRESH_A && !alternatorOnByV(V))
        rest_accum_s += (now - lastSampleMs) / 1000.0f;
    else
        rest_accum_s = 0.0f;


    // Parked/Idle detection
    bool altOn    = alternatorOnByV(V);
    bool activity = recentStepActivity(I, now);
    if (!altOn && fabsf(I) < BASE_CONS_THRESH_A && !activity) {
      lowCurrentAccum_s += (now - lastSampleMs) / 1000.0f;
    } else {
      lowCurrentAccum_s  = 0.0f;
      if (mode == MODE_PARKED_IDLE) mode = MODE_ACTIVE; // exit parked-idle on activity
    }

    // Enter Parked&Idle after dwell
    if (mode == MODE_ACTIVE && lowCurrentAccum_s >= (float)PARKED_IDLE_ENTRY_DWELL_SEC) {
      mode = MODE_PARKED_IDLE;
      parkedIdleEnterMs = now;
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        mqtt.publish(MQTT_TOPIC, "{\"mode\":\"parked-idle\"}", true);
        mqtt.loop();
      }
    }


    // Clamp SOC
    if (!isfinite(soc_pct)) soc_pct = 50.0f;
    soc_pct = fmaxf(0.0f, fminf(100.0f, soc_pct));

    // Update “last” values once per tick (canonical spot)
    if (isfinite(V)) last_V_V = V;
    last_I_A     = I;
    lastSampleMs = now;

    // Feed learner with the current sample (either V/I or last_*)
    learner.ingest(last_V_V, last_I_A, (isfinite(last_T_C) ? last_T_C : NAN), now);

  
  
  }

  // --- Temperature @1 Hz ---
  if (now - lastTempMs >= TEMP_INTERVAL_MS) {
    float tC = ds.readTempC();
    if (isfinite(tC)) last_T_C = tC;


    bool altOff = !alternatorOnByV(last_V_V);
    if (altOff && rest_accum_s >= (float)REST_DETECT_SEC && isfinite(last_V_V)) {
        float v25 = compensateOCVTo25C(last_V_V, last_T_C);
        float ocvSOC = socFromOCV_25C(v25);
        soc_pct = 0.8f * soc_pct + 0.2f * ocvSOC;
        soc_pct = fmaxf(0.0f, fminf(100.0f, soc_pct));
    }
    lastTempMs = now;    
  
  }



  


  // --- Publish after sampling ---

  if (now - lastPublishMs >= publishInterval) {

    if (!wifi.connected()) wifi.connectSmart(WIFI_SSID, WIFI_PASSWORD, 10000, 5);
    if (wifi.connected() && !mqtt.connected()) mqtt.ensureConnected(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
     

     Serial.println("prepare to send");

    bool altOn = alternatorOnByV(last_V_V);
    TelemetryFrame tf {
      .mode = (mode == MODE_ACTIVE ? "active" : "parked-idle"),
      .V = last_V_V, .I = last_I_A, .T = last_T_C,
      .soc_pct = soc_pct, .soh_pct = learner.currentSOH() * 100.0f,
      .Rint_mOhm = learner.lastRint_mOhm(), .Rint25_mOhm = learner.lastRint25_mOhm(),
      .RintBaseline_mOhm = learner.baseline_mOhm(),
      .alternator_on = altOn,
      .rest_s = (uint32_t)rest_accum_s,
      .lowCurrentAccum_s = (uint32_t)lowCurrentAccum_s,
      .up_ms = now,
      .hasRint = isfinite(learner.lastRint_mOhm()),
      .hasRint25 = isfinite(learner.lastRint25_mOhm())
    };

    char payload[700];
    if (buildTelemetryJson(tf, payload, sizeof(payload))) {
      Serial.printf("Payload length: %u", (unsigned)strlen(payload));
      mqtt.publish(MQTT_TOPIC, payload, false);
      mqtt.loop();
    }
      

      ble.update(last_V_V, last_I_A, last_T_C, tf.mode);

      if (mqtt.connected()) {

    }
    lastPublishMs = now;
  }

  // --- 5h timer: Parked&Idle → Deep Sleep (10 min cadence) ---
  if (mode == MODE_PARKED_IDLE) {
    if ((now - parkedIdleEnterMs) >= PARKED_IDLE_MAX_MS) {
      if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "{\"mode\":\"parked-sleep\",\"sleep_s\":%lu}",
                 (unsigned long)(PARKED_WAKE_INTERVAL_US/1000000ULL));
        mqtt.publish(MQTT_TOPIC, msg, true);
        mqtt.loop();
        delay(50);
      }
      goToDeepSleep(PARKED_WAKE_INTERVAL_US);
    }
  }
  


  mqtt.loop();
}
