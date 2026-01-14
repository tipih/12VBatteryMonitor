#include <cstdint>
#include <string>
#include <unity.h>
#include <vector>

// Mock tracking structure
struct MockCallLog {
  std::string action;
  uint64_t param;
};

std::vector<MockCallLog> mockCalls;

// Mock ESP32 sleep and wireless functions
namespace MockESP {
void esp_sleep_enable_timer_wakeup(uint64_t time_in_us) {
  mockCalls.push_back({"esp_sleep_enable_timer_wakeup", time_in_us});
}

void esp_deep_sleep_start() {
  mockCalls.push_back({"esp_deep_sleep_start", 0});
}

void btStop() { mockCalls.push_back({"btStop", 0}); }
} // namespace MockESP

class MockWiFi {
public:
  static void disconnect(bool wifioff, bool eraseap) {
    mockCalls.push_back(
        {"WiFi.disconnect", (uint64_t)((wifioff ? 1 : 0) | (eraseap ? 2 : 0))});
  }

  static void mode(int m) { mockCalls.push_back({"WiFi.mode", (uint64_t)m}); }
};

class MockNimBLEDevice {
public:
  static void deinit(bool clearAll) {
    mockCalls.push_back({"NimBLEDevice.deinit", (uint64_t)(clearAll ? 1 : 0)});
  }
};

// Power/sleep manager implementation for testing
class SleepManager {
public:
  enum class WakeupReason { UNKNOWN, TIMER, EXTERNAL, TOUCHPAD };

  // Prepare for deep sleep with timer wakeup
  void prepareDeepSleep(uint64_t interval_us) {
    _wakeupInterval = interval_us;
    _prepared = true;
  }

  // Execute deep sleep (in real hardware this never returns)
  void enterDeepSleep() {
    if (!_prepared)
      return;

    // Shutdown wireless
    MockNimBLEDevice::deinit(true);
    MockWiFi::disconnect(true, true);
    MockWiFi::mode(0); // WIFI_OFF
    MockESP::btStop();

    // Configure wakeup
    MockESP::esp_sleep_enable_timer_wakeup(_wakeupInterval);

    // Enter deep sleep
    MockESP::esp_deep_sleep_start();

    _prepared = false;
    _sleepCount++;
  }

  // Combined prepare + enter for convenience
  void goToDeepSleep(uint64_t interval_us) {
    prepareDeepSleep(interval_us);
    enterDeepSleep();
  }

  // Query methods
  bool isPrepared() const { return _prepared; }
  uint64_t getWakeupInterval() const { return _wakeupInterval; }
  int getSleepCount() const { return _sleepCount; }

  // Convert microseconds to minutes for readability
  static float usToMinutes(uint64_t us) { return us / 60000000.0f; }

  static uint64_t minutesToUs(float minutes) {
    return (uint64_t)(minutes * 60000000.0f);
  }

  void reset() {
    _prepared = false;
    _wakeupInterval = 0;
    _sleepCount = 0;
  }

private:
  bool _prepared = false;
  uint64_t _wakeupInterval = 0;
  int _sleepCount = 0;
};

// Test cases
void setUp(void) { mockCalls.clear(); }

void tearDown(void) {}

void test_sleep_manager_initialization(void) {
  SleepManager mgr;

  TEST_ASSERT_FALSE(mgr.isPrepared());
  TEST_ASSERT_EQUAL_UINT64(0, mgr.getWakeupInterval());
  TEST_ASSERT_EQUAL(0, mgr.getSleepCount());
}

void test_sleep_manager_prepare_deep_sleep(void) {
  SleepManager mgr;

  // Prepare for 5 minute sleep
  uint64_t fiveMinutes = SleepManager::minutesToUs(5.0f);
  mgr.prepareDeepSleep(fiveMinutes);

  TEST_ASSERT_TRUE(mgr.isPrepared());
  TEST_ASSERT_EQUAL_UINT64(fiveMinutes, mgr.getWakeupInterval());

  // No calls should be made yet
  TEST_ASSERT_EQUAL(0, mockCalls.size());
}

void test_sleep_manager_enter_deep_sleep(void) {
  SleepManager mgr;

  uint64_t interval = SleepManager::minutesToUs(10.0f);
  mgr.prepareDeepSleep(interval);
  mgr.enterDeepSleep();

  // Should not be prepared after entering sleep
  TEST_ASSERT_FALSE(mgr.isPrepared());
  TEST_ASSERT_EQUAL(1, mgr.getSleepCount());

  // Verify all shutdown calls were made in correct order
  TEST_ASSERT_EQUAL(6, mockCalls.size());
  TEST_ASSERT_EQUAL_STRING("NimBLEDevice.deinit", mockCalls[0].action.c_str());
  TEST_ASSERT_EQUAL_STRING("WiFi.disconnect", mockCalls[1].action.c_str());
  TEST_ASSERT_EQUAL_STRING("WiFi.mode", mockCalls[2].action.c_str());
  TEST_ASSERT_EQUAL_STRING("btStop", mockCalls[3].action.c_str());
  TEST_ASSERT_EQUAL_STRING("esp_sleep_enable_timer_wakeup",
                           mockCalls[4].action.c_str());
  TEST_ASSERT_EQUAL_UINT64(interval, mockCalls[4].param);
  TEST_ASSERT_EQUAL_STRING("esp_deep_sleep_start", mockCalls[5].action.c_str());
}

void test_sleep_manager_enter_without_prepare(void) {
  SleepManager mgr;

  // Try to enter sleep without preparing
  mgr.enterDeepSleep();

  // Should not do anything
  TEST_ASSERT_EQUAL(0, mockCalls.size());
  TEST_ASSERT_EQUAL(0, mgr.getSleepCount());
}

void test_sleep_manager_combined_go_to_deep_sleep(void) {
  SleepManager mgr;

  uint64_t interval = SleepManager::minutesToUs(3.0f);
  mgr.goToDeepSleep(interval);

  TEST_ASSERT_EQUAL(1, mgr.getSleepCount());
  TEST_ASSERT_EQUAL(6, mockCalls.size());
  TEST_ASSERT_EQUAL_UINT64(interval, mockCalls[4].param);
}

void test_sleep_manager_multiple_sleep_cycles(void) {
  SleepManager mgr;

  // First sleep cycle
  mgr.goToDeepSleep(SleepManager::minutesToUs(5.0f));
  TEST_ASSERT_EQUAL(1, mgr.getSleepCount());

  mockCalls.clear();

  // Second sleep cycle
  mgr.goToDeepSleep(SleepManager::minutesToUs(10.0f));
  TEST_ASSERT_EQUAL(2, mgr.getSleepCount());
  TEST_ASSERT_EQUAL(6, mockCalls.size());
}

void test_sleep_manager_time_conversions(void) {
  // Test microsecond to minute conversion
  uint64_t oneMinute = 60000000ULL;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, SleepManager::usToMinutes(oneMinute));

  uint64_t fiveMinutes = 300000000ULL;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, SleepManager::usToMinutes(fiveMinutes));

  // Test minute to microsecond conversion
  TEST_ASSERT_EQUAL_UINT64(oneMinute, SleepManager::minutesToUs(1.0f));
  TEST_ASSERT_EQUAL_UINT64(fiveMinutes, SleepManager::minutesToUs(5.0f));
}

void test_sleep_manager_common_intervals(void) {
  SleepManager mgr;

  // Test common sleep intervals used in the project

  // 5 minute parked wake interval
  uint64_t fiveMin = SleepManager::minutesToUs(5.0f);
  mgr.goToDeepSleep(fiveMin);
  TEST_ASSERT_EQUAL_UINT64(fiveMin, mockCalls[4].param);

  mockCalls.clear();
  mgr.reset();

  // 10 minute deep sleep
  uint64_t tenMin = SleepManager::minutesToUs(10.0f);
  mgr.goToDeepSleep(tenMin);
  TEST_ASSERT_EQUAL_UINT64(tenMin, mockCalls[4].param);
}

void test_sleep_manager_nimble_shutdown_parameter(void) {
  SleepManager mgr;

  mgr.goToDeepSleep(SleepManager::minutesToUs(1.0f));

  // Verify NimBLE deinit was called with clearAll=true
  TEST_ASSERT_EQUAL(1, mockCalls[0].param);
}

void test_sleep_manager_wifi_shutdown_parameters(void) {
  SleepManager mgr;

  mgr.goToDeepSleep(SleepManager::minutesToUs(1.0f));

  // Verify WiFi.disconnect was called with wifioff=true, eraseap=true
  // param encoding: bit 0 = wifioff, bit 1 = eraseap
  TEST_ASSERT_EQUAL(3, mockCalls[1].param); // 0b11 = both true

  // Verify WiFi.mode was called with WIFI_OFF (0)
  TEST_ASSERT_EQUAL(0, mockCalls[2].param);
}

void test_sleep_manager_reset(void) {
  SleepManager mgr;

  mgr.prepareDeepSleep(SleepManager::minutesToUs(5.0f));
  TEST_ASSERT_TRUE(mgr.isPrepared());

  mgr.reset();

  TEST_ASSERT_FALSE(mgr.isPrepared());
  TEST_ASSERT_EQUAL_UINT64(0, mgr.getWakeupInterval());
  TEST_ASSERT_EQUAL(0, mgr.getSleepCount());
}

void test_sleep_manager_very_long_sleep(void) {
  SleepManager mgr;

  // Test 60 minute sleep (1 hour)
  uint64_t oneHour = SleepManager::minutesToUs(60.0f);
  mgr.goToDeepSleep(oneHour);

  TEST_ASSERT_EQUAL_UINT64(oneHour, mockCalls[4].param);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, SleepManager::usToMinutes(oneHour));
}

void test_sleep_manager_very_short_sleep(void) {
  SleepManager mgr;

  // Test 30 second sleep (0.5 minutes)
  uint64_t halfMinute = SleepManager::minutesToUs(0.5f);
  mgr.goToDeepSleep(halfMinute);

  TEST_ASSERT_EQUAL_UINT64(halfMinute, mockCalls[4].param);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, SleepManager::usToMinutes(halfMinute));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_sleep_manager_initialization);
  RUN_TEST(test_sleep_manager_prepare_deep_sleep);
  RUN_TEST(test_sleep_manager_enter_deep_sleep);
  RUN_TEST(test_sleep_manager_enter_without_prepare);
  RUN_TEST(test_sleep_manager_combined_go_to_deep_sleep);
  RUN_TEST(test_sleep_manager_multiple_sleep_cycles);
  RUN_TEST(test_sleep_manager_time_conversions);
  RUN_TEST(test_sleep_manager_common_intervals);
  RUN_TEST(test_sleep_manager_nimble_shutdown_parameter);
  RUN_TEST(test_sleep_manager_wifi_shutdown_parameters);
  RUN_TEST(test_sleep_manager_reset);
  RUN_TEST(test_sleep_manager_very_long_sleep);
  RUN_TEST(test_sleep_manager_very_short_sleep);

  return UNITY_END();
}
