#include <cmath>
#include <map>
#include <string>
#include <unity.h>

// Mock Arduino dependencies
#define isfinite std::isfinite
void delay(int ms) { /* no-op for testing */ }

// Mock global capacity variable
float batteryCapacityAh = 9.0f;

// Mock Preferences class with in-memory storage
class Preferences {
public:
  static std::map<std::string, std::map<std::string, float>> storage;
  std::string currentNamespace;

  bool begin(const char *name, bool readOnly) {
    currentNamespace = name;
    return true;
  }

  void end() {}

  bool isKey(const char *key) {
    if (storage.find(currentNamespace) == storage.end())
      return false;
    return storage[currentNamespace].find(key) !=
           storage[currentNamespace].end();
  }

  float getFloat(const char *key, float defaultValue) {
    if (!isKey(key))
      return defaultValue;
    return storage[currentNamespace][key];
  }

  bool putFloat(const char *key, float value) {
    storage[currentNamespace][key] = value;
    return true; // Success
  }

  static void clear() { storage.clear(); }
};

std::map<std::string, std::map<std::string, float>> Preferences::storage;

// Mock BLE notification
std::string last_ble_notification;
void ble_notify_status(const char *msg) { last_ble_notification = msg; }

// Constants
#define BATTERY_CAPACITY_AH 9.0f

// Include BatteryConfig implementation
class BatteryConfig {
public:
  void begin();
  void setCapacity(float ah);
};

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

// Test cases
void setUp(void) {
  Preferences::clear();
  batteryCapacityAh = 9.0f;
  last_ble_notification.clear();
}

void tearDown(void) {}

void test_battery_config_begin_no_stored_value(void) {
  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.begin();

  // Should keep default value when nothing stored
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);
}

void test_battery_config_begin_with_primary_key(void) {
  // Store a value in NVS
  Preferences prefs;
  prefs.begin("battmon", false);
  prefs.putFloat("bat_cap", 12.5f);
  prefs.end();

  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.begin();

  // Should load stored value
  TEST_ASSERT_EQUAL_FLOAT(12.5f, batteryCapacityAh);
}

void test_battery_config_begin_with_fallback_key(void) {
  // Store a value only in fallback key
  Preferences prefs;
  prefs.begin("battmon", false);
  prefs.putFloat("bat_cap2", 15.0f);
  prefs.end();

  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.begin();

  // Should load fallback value
  TEST_ASSERT_EQUAL_FLOAT(15.0f, batteryCapacityAh);
}

void test_battery_config_begin_primary_over_fallback(void) {
  // Store values in both keys
  Preferences prefs;
  prefs.begin("battmon", false);
  prefs.putFloat("bat_cap", 12.0f);
  prefs.putFloat("bat_cap2", 15.0f);
  prefs.end();

  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.begin();

  // Should prefer primary key
  TEST_ASSERT_EQUAL_FLOAT(12.0f, batteryCapacityAh);
}

void test_battery_config_set_capacity_valid(void) {
  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.setCapacity(15.5f);

  // Global variable should be updated
  TEST_ASSERT_EQUAL_FLOAT(15.5f, batteryCapacityAh);

  // Should be stored in NVS
  Preferences prefs;
  prefs.begin("battmon", false);
  TEST_ASSERT_TRUE(prefs.isKey("bat_cap"));
  TEST_ASSERT_EQUAL_FLOAT(15.5f, prefs.getFloat("bat_cap", 0.0f));
  prefs.end();

  // Should send BLE notification
  TEST_ASSERT_TRUE(last_ble_notification.find("put=1") != std::string::npos);
  TEST_ASSERT_TRUE(last_ble_notification.find("CAP=15.500") !=
                   std::string::npos);
}

void test_battery_config_set_capacity_invalid_values(void) {
  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  // Negative value
  config.setCapacity(-5.0f);
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);

  // Zero
  config.setCapacity(0.0f);
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);

  // NaN
  config.setCapacity(NAN);
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);

  // Infinity
  config.setCapacity(INFINITY);
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);
}

void test_battery_config_persistence_roundtrip(void) {
  BatteryConfig config;

  // Set and store capacity
  config.setCapacity(20.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, batteryCapacityAh);

  // Reset global variable
  batteryCapacityAh = 9.0f;

  // Load from storage
  config.begin();
  TEST_ASSERT_EQUAL_FLOAT(20.0f, batteryCapacityAh);
}

void test_battery_config_begin_ignores_invalid_stored_values(void) {
  // Store invalid values
  Preferences prefs;
  prefs.begin("battmon", false);
  prefs.putFloat("bat_cap", -5.0f);
  prefs.end();

  BatteryConfig config;
  batteryCapacityAh = 9.0f;

  config.begin();

  // Should keep default value when stored value is invalid
  TEST_ASSERT_EQUAL_FLOAT(9.0f, batteryCapacityAh);
}

void test_battery_config_multiple_sets(void) {
  BatteryConfig config;

  config.setCapacity(10.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, batteryCapacityAh);

  config.setCapacity(12.5f);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, batteryCapacityAh);

  config.setCapacity(15.0f);
  TEST_ASSERT_EQUAL_FLOAT(15.0f, batteryCapacityAh);

  // Verify last value is stored
  Preferences prefs;
  prefs.begin("battmon", false);
  TEST_ASSERT_EQUAL_FLOAT(15.0f, prefs.getFloat("bat_cap", 0.0f));
  prefs.end();
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_battery_config_begin_no_stored_value);
  RUN_TEST(test_battery_config_begin_with_primary_key);
  RUN_TEST(test_battery_config_begin_with_fallback_key);
  RUN_TEST(test_battery_config_begin_primary_over_fallback);
  RUN_TEST(test_battery_config_set_capacity_valid);
  RUN_TEST(test_battery_config_set_capacity_invalid_values);
  RUN_TEST(test_battery_config_persistence_roundtrip);
  RUN_TEST(test_battery_config_begin_ignores_invalid_stored_values);
  RUN_TEST(test_battery_config_multiple_sets);

  return UNITY_END();
}
