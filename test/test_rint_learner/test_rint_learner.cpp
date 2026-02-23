#include <cmath>
#include <cstring>
#include <unity.h>

// Mock Arduino dependencies for native testing
static unsigned long mock_time = 0;

unsigned long millis() { return mock_time; }

void set_mock_millis(unsigned long time) { mock_time = time; }

// Mock Preferences class
class Preferences {
public:
  bool begin(const char *name, bool readOnly) { return true; }
  void end() {}
  float getFloat(const char *key, float defaultValue) { return defaultValue; }
  void putFloat(const char *key, float value) {}
};

// Include app_config constants we need
#define INITIAL_BASELINE_mOHM 35.0f
#define ALT_ON_VOLTAGE_V 13.5f
#define TEMP_ALPHA_PER_C 0.005f
#define REF_TEMP_C 25.0f
#define MAX_RINT25_mOHM 200.0f
#define MIN_RINT25_mOHM 3.0f
#define MIN_RINT25_VS_BASELINE_RATIO 0.4f

// Mock DebugPublisher
class DebugPublisher {
public:
  bool ok() const { return false; }
  void send(const char *msg, const char *tag) {}
};

// Now include the rint_learner header (simplified version for testing)
struct Sample {
  float V;
  float I;
  float T;
  uint32_t t;
};

class RintLearnerSimple {
public:
  void begin(float initialBaseline_mOhm) {
    _baseline_mOhm = initialBaseline_mOhm;
    _lastRint_mOhm = NAN;
    _lastRint25_mOhm = NAN;
  }

  float baseline_mOhm() const { return _baseline_mOhm; }
  float lastRint_mOhm() const { return _lastRint_mOhm; }
  float lastRint25_mOhm() const { return _lastRint25_mOhm; }

  void setBaseline(float baseline_mOhm) {
    if (!isfinite(baseline_mOhm) || baseline_mOhm <= 0.1f ||
        baseline_mOhm > 1000.0f)
      return;
    _baseline_mOhm = baseline_mOhm;
  }

  float currentSOH() const {
    if (!isfinite(_lastRint25_mOhm) || _lastRint25_mOhm < MIN_RINT25_mOHM ||
        _lastRint25_mOhm > MAX_RINT25_mOHM)
      return 1.0f;
    float soh = _baseline_mOhm / _lastRint25_mOhm;
    if (soh < 0.0f)
      soh = 0.0f;
    if (soh > 1.0f)
      soh = 1.0f;
    return soh;
  }

  // Simplified method to set measured values for testing
  void setMeasuredRint(float rint_mOhm, float temp_c) {
    _lastRint_mOhm = rint_mOhm;
    float r25 = compTo25C(rint_mOhm, temp_c);
    
    // Apply same validation as the real learner
    if (!isfinite(r25) || r25 <= 0.0f || r25 > MAX_RINT25_mOHM) {
      _lastRint25_mOhm = NAN;
      return;
    }
    
    // Reject unrealistically low measurements
    if (r25 < MIN_RINT25_mOHM) {
      _lastRint25_mOhm = NAN;
      return;
    }
    
    // Reject measurements significantly below baseline
    float minAcceptable = _baseline_mOhm * MIN_RINT25_VS_BASELINE_RATIO;
    if (r25 < minAcceptable) {
      _lastRint25_mOhm = NAN;
      return;
    }
    
    _lastRint25_mOhm = r25;
  }

private:
  float _baseline_mOhm = INITIAL_BASELINE_mOHM;
  float _lastRint_mOhm = NAN;
  float _lastRint25_mOhm = NAN;

  static float compTo25C(float R_mOhm, float tempC) {
    // Handle invalid temperatures
    if (!isfinite(tempC))
      return R_mOhm; // No compensation if temp is invalid

    float f = 1.0f + TEMP_ALPHA_PER_C * (tempC - REF_TEMP_C);
    if (f < 0.5f)
      f = 0.5f;
    if (f > 1.5f)
      f = 1.5f;
    return R_mOhm / f;
  }
};

// Test cases
void setUp(void) {
  // Set up code before each test
}

void tearDown(void) {
  // Clean up code after each test
}

void test_rint_learner_initialization(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.baseline_mOhm());
  TEST_ASSERT_TRUE(isnan(learner.lastRint_mOhm()));
  TEST_ASSERT_TRUE(isnan(learner.lastRint25_mOhm()));
}

void test_rint_learner_set_baseline(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  learner.setBaseline(40.0f);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, learner.baseline_mOhm());

  // Test invalid values are rejected
  learner.setBaseline(-5.0f);
  TEST_ASSERT_EQUAL_FLOAT(40.0f,
                          learner.baseline_mOhm()); // Should remain unchanged

  learner.setBaseline(0.05f);
  TEST_ASSERT_EQUAL_FLOAT(40.0f,
                          learner.baseline_mOhm()); // Should remain unchanged

  learner.setBaseline(2000.0f);
  TEST_ASSERT_EQUAL_FLOAT(40.0f,
                          learner.baseline_mOhm()); // Should remain unchanged
}

void test_temperature_compensation(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // At 25°C, Rint should equal Rint25
  learner.setMeasuredRint(35.0f, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.lastRint_mOhm());
  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.lastRint25_mOhm());

  // At higher temperature, Rint25 should be lower than measured Rint
  learner.setMeasuredRint(35.0f, 35.0f);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.lastRint_mOhm());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 33.4f,
                           learner.lastRint25_mOhm()); // Approx 35/(1+0.005*10)

  // At lower temperature, Rint25 should be higher than measured Rint
  learner.setMeasuredRint(35.0f, 15.0f);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.lastRint_mOhm());
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 36.8f,
                           learner.lastRint25_mOhm()); // Approx 35/(1-0.005*10)
}

void test_soh_calculation(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // No measurement yet, should return 1.0 (100%)
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Rint25 equals baseline: SOH = 100%
  learner.setMeasuredRint(35.0f, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Rint25 higher than baseline (degraded): SOH < 100%
  learner.setMeasuredRint(46.7f, 25.0f); // Rint25 = 46.7, baseline = 35
  float expected_soh = 35.0f / 46.7f;    // ~0.75
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_soh, learner.currentSOH());

  // Rint25 lower than baseline (better than baseline): SOH capped at 100%
  learner.setMeasuredRint(30.0f, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f,
                          learner.currentSOH()); // Should be capped at 1.0
}

void test_soh_with_temperature_variation(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // Simulate measurement at 30°C with actual Rint of 40 mOhm
  // After temp compensation, Rint25 should be ~38.1 mOhm
  // SOH = 35.0 / 38.1 ≈ 0.92 (92%)
  learner.setMeasuredRint(40.0f, 30.0f);
  float soh = learner.currentSOH();
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.92f, soh);
}

void test_extreme_temperatures(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // Sensor failure: NAN temperature
  learner.setMeasuredRint(40.0f, NAN);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, learner.lastRint_mOhm());
  // Rint25 should still be calculated (using clamped temp factor)
  TEST_ASSERT_TRUE(isfinite(learner.lastRint25_mOhm()));

  // Extreme cold (-50°C) - temp factor gets clamped
  learner.setMeasuredRint(40.0f, -50.0f);
  TEST_ASSERT_TRUE(isfinite(learner.lastRint25_mOhm()));
  TEST_ASSERT_TRUE(learner.lastRint25_mOhm() > 0.0f);

  // Extreme hot (100°C) - temp factor gets clamped
  learner.setMeasuredRint(40.0f, 100.0f);
  TEST_ASSERT_TRUE(isfinite(learner.lastRint25_mOhm()));
  TEST_ASSERT_TRUE(learner.lastRint25_mOhm() > 0.0f);
}

void test_division_by_zero_protection(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // Set Rint25 to zero - should not crash when calculating SOH
  learner.setMeasuredRint(0.0f, 25.0f);
  float soh = learner.currentSOH();
  // Should return 1.0 when Rint25 is invalid (zero or negative)
  TEST_ASSERT_EQUAL_FLOAT(1.0f, soh);

  // Negative Rint25 (shouldn't happen but test safety)
  learner.setMeasuredRint(-10.0f, 25.0f);
  soh = learner.currentSOH();
  TEST_ASSERT_EQUAL_FLOAT(1.0f, soh);
}

void test_infinite_rint_values(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // Sensor error: infinite resistance
  learner.setMeasuredRint(INFINITY, 25.0f);
  // Should not propagate infinity
  float soh = learner.currentSOH();
  TEST_ASSERT_EQUAL_FLOAT(1.0f, soh); // Default when no valid measurement

  // Negative infinity
  learner.setMeasuredRint(-INFINITY, 25.0f);
  soh = learner.currentSOH();
  TEST_ASSERT_EQUAL_FLOAT(1.0f, soh);
}

void test_lower_bound_rint_validation(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f); // baseline = 35 mΩ

  // Test Case 1: Unrealistically low value (< MIN_RINT25_mOHM = 3.0)
  // This simulates the 5.96 mΩ reading from the bug report
  learner.setMeasuredRint(2.5f, 25.0f);
  TEST_ASSERT_TRUE(isnan(learner.lastRint25_mOhm())); // Should be rejected
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH()); // Should return 100%

  // Test Case 2: Value below 40% of baseline (< 14 mΩ for baseline of 35 mΩ)
  // This prevents accepting measurements significantly below baseline
  learner.setMeasuredRint(10.0f, 25.0f); // Below 40% * 35 = 14 mΩ
  TEST_ASSERT_TRUE(isnan(learner.lastRint25_mOhm())); // Should be rejected
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Test Case 3: Just above the threshold (should be accepted)
  learner.setMeasuredRint(15.0f, 25.0f); // Above 14 mΩ threshold
  TEST_ASSERT_FALSE(isnan(learner.lastRint25_mOhm())); // Should be accepted
  TEST_ASSERT_EQUAL_FLOAT(15.0f, learner.lastRint25_mOhm());
  // SOH = 35/15 = 2.33, capped at 1.0
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Test Case 4: Normal healthy battery reading
  learner.setMeasuredRint(35.0f, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, learner.lastRint25_mOhm());
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Test Case 5: Degraded battery (high resistance)
  learner.setMeasuredRint(58.65f, 25.0f); // From the bug report
  TEST_ASSERT_EQUAL_FLOAT(58.65f, learner.lastRint25_mOhm());
  float expected_soh = 35.0f / 58.65f; // ~0.597 or 59.7%
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_soh, learner.currentSOH());

  // Test Case 6: Value at exactly MIN_RINT25_mOHM (boundary test)
  learner.setMeasuredRint(MIN_RINT25_mOHM, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(MIN_RINT25_mOHM, learner.lastRint25_mOhm());

  // Test Case 7: Value just below MIN_RINT25_mOHM (should be rejected)
  learner.setMeasuredRint(MIN_RINT25_mOHM - 0.1f, 25.0f);
  TEST_ASSERT_TRUE(isnan(learner.lastRint25_mOhm()));
}

void test_upper_bound_rint_validation(void) {
  RintLearnerSimple learner;
  learner.begin(35.0f);

  // Test Case 1: Value above MAX_RINT25_mOHM (should be rejected)
  learner.setMeasuredRint(250.0f, 25.0f);
  TEST_ASSERT_TRUE(isnan(learner.lastRint25_mOhm()));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, learner.currentSOH());

  // Test Case 2: Value at MAX_RINT25_mOHM (boundary test, should be accepted)
  learner.setMeasuredRint(MAX_RINT25_mOHM, 25.0f);
  TEST_ASSERT_EQUAL_FLOAT(MAX_RINT25_mOHM, learner.lastRint25_mOhm());
  float expected_soh = 35.0f / MAX_RINT25_mOHM;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_soh, learner.currentSOH());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_rint_learner_initialization);
  RUN_TEST(test_rint_learner_set_baseline);
  RUN_TEST(test_temperature_compensation);
  RUN_TEST(test_soh_calculation);
  RUN_TEST(test_soh_with_temperature_variation);
  RUN_TEST(test_extreme_temperatures);
  RUN_TEST(test_division_by_zero_protection);
  RUN_TEST(test_infinite_rint_values);
  RUN_TEST(test_lower_bound_rint_validation);
  RUN_TEST(test_upper_bound_rint_validation);

  return UNITY_END();
}
