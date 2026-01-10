#include <unity.h>
#include <cmath>

// Mock Arduino dependencies
#define isfinite std::isfinite
#define fabsf std::fabs

// Constants from app_config.h
#define ALT_ON_VOLTAGE_V 13.2f
#define STEP_ACTIVITY_DI_A 1.5f
#define STEP_ACTIVITY_WINDOW_MS 200

// Include the BatteryStateDetector implementation inline for testing
class BatteryStateDetector {
public:
    bool alternatorOn(float V);
    bool hasRecentActivity(float currentA, uint32_t nowMs);
    void reset();

private:
    bool _altState = false;
    float _prevCurrent = NAN;
    uint32_t _prevMs = 0;
};

bool BatteryStateDetector::alternatorOn(float V) {
    const float ON_THRESH = ALT_ON_VOLTAGE_V;
    const float OFF_THRESH = ON_THRESH - 0.25f;

    if (_altState)
        _altState = (V >= OFF_THRESH);
    else
        _altState = (V >= ON_THRESH);

    return _altState;
}

bool BatteryStateDetector::hasRecentActivity(float currentA, uint32_t nowMs) {
    if (!isfinite(_prevCurrent)) {
        _prevCurrent = currentA;
        _prevMs = nowMs;
        return false;
    }

    float dI = fabsf(currentA - _prevCurrent);
    uint32_t dt = nowMs - _prevMs;
    bool active = (dI >= STEP_ACTIVITY_DI_A && dt <= STEP_ACTIVITY_WINDOW_MS);

    _prevCurrent = currentA;
    _prevMs = nowMs;

    return active;
}

void BatteryStateDetector::reset() {
    _altState = false;
    _prevCurrent = NAN;
    _prevMs = 0;
}

// Test cases
void setUp(void) {}
void tearDown(void) {}

void test_alternator_initial_state(void) {
    BatteryStateDetector detector;
    
    // Initial state should be off
    TEST_ASSERT_FALSE(detector.alternatorOn(12.5f));
}

void test_alternator_turn_on(void) {
    BatteryStateDetector detector;
    
    // Below threshold - should be off
    TEST_ASSERT_FALSE(detector.alternatorOn(13.0f));
    
    // At threshold - should turn on
    TEST_ASSERT_TRUE(detector.alternatorOn(13.2f));
    
    // Above threshold - should stay on
    TEST_ASSERT_TRUE(detector.alternatorOn(14.0f));
}

void test_alternator_hysteresis(void) {
    BatteryStateDetector detector;
    
    // Turn alternator on
    TEST_ASSERT_TRUE(detector.alternatorOn(13.5f));
    
    // Drop slightly below ON threshold but above OFF threshold (13.2 - 0.25 = 12.95)
    // Should stay ON due to hysteresis
    TEST_ASSERT_TRUE(detector.alternatorOn(13.1f));
    TEST_ASSERT_TRUE(detector.alternatorOn(13.0f));
    
    // Drop below OFF threshold - should turn off
    TEST_ASSERT_FALSE(detector.alternatorOn(12.9f));
    
    // Rise back but below ON threshold - should stay off
    TEST_ASSERT_FALSE(detector.alternatorOn(13.0f));
    TEST_ASSERT_FALSE(detector.alternatorOn(13.1f));
    
    // Rise to ON threshold - should turn back on
    TEST_ASSERT_TRUE(detector.alternatorOn(13.2f));
}

void test_alternator_reset(void) {
    BatteryStateDetector detector;
    
    // Turn on
    TEST_ASSERT_TRUE(detector.alternatorOn(13.5f));
    
    // Reset
    detector.reset();
    
    // Should be off after reset
    TEST_ASSERT_FALSE(detector.alternatorOn(13.0f));
}

void test_activity_first_call(void) {
    BatteryStateDetector detector;
    
    // First call should always return false (no previous data)
    TEST_ASSERT_FALSE(detector.hasRecentActivity(5.0f, 1000));
}

void test_activity_no_change(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // No significant change in current
    TEST_ASSERT_FALSE(detector.hasRecentActivity(5.1f, 1100));
}

void test_activity_large_change_within_window(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // Large current change within time window
    // ΔI = |7.0 - 5.0| = 2.0A >= 1.5A threshold
    // Δt = 1150 - 1000 = 150ms <= 200ms window
    TEST_ASSERT_TRUE(detector.hasRecentActivity(7.0f, 1150));
}

void test_activity_large_change_outside_window(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // Large current change but outside time window
    // ΔI = 2.0A >= threshold but Δt = 250ms > 200ms window
    TEST_ASSERT_FALSE(detector.hasRecentActivity(7.0f, 1250));
}

void test_activity_small_change_within_window(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // Small current change within time window
    // ΔI = 1.0A < 1.5A threshold
    TEST_ASSERT_FALSE(detector.hasRecentActivity(6.0f, 1100));
}

void test_activity_threshold_boundary(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // Exactly at threshold (1.5A change)
    TEST_ASSERT_TRUE(detector.hasRecentActivity(6.5f, 1100));
}

void test_activity_negative_change(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(7.0f, 1000);
    
    // Large negative change (discharge to charge transition)
    // ΔI = |4.5 - 7.0| = 2.5A
    TEST_ASSERT_TRUE(detector.hasRecentActivity(4.5f, 1150));
}

void test_activity_multiple_steps(void) {
    BatteryStateDetector detector;
    
    // Initialize
    detector.hasRecentActivity(5.0f, 1000);
    
    // First step
    TEST_ASSERT_TRUE(detector.hasRecentActivity(7.0f, 1100));
    
    // Small change after first step
    TEST_ASSERT_FALSE(detector.hasRecentActivity(7.2f, 1200));
    
    // Another large step
    TEST_ASSERT_TRUE(detector.hasRecentActivity(9.0f, 1300));
}

void test_activity_reset(void) {
    BatteryStateDetector detector;
    
    // Initialize with data
    detector.hasRecentActivity(5.0f, 1000);
    detector.hasRecentActivity(7.0f, 1100);
    
    // Reset
    detector.reset();
    
    // After reset, first call should return false
    TEST_ASSERT_FALSE(detector.hasRecentActivity(5.0f, 2000));
}

void test_activity_millis_rollover(void) {
    BatteryStateDetector detector;
    
    // Initialize near max uint32_t (millis() rollover happens every 49.7 days)
    uint32_t near_max = 0xFFFFFF00; // Close to UINT32_MAX
    detector.hasRecentActivity(5.0f, near_max);
    
    // Simulate rollover: time wraps to small value
    // In real code, dt calculation would underflow/overflow
    // This tests that we handle the rollover gracefully
    uint32_t after_rollover = 100; // Wrapped around
    
    // Large current change but time delta is huge due to rollover
    // Should return false (outside window due to apparent huge time gap)
    TEST_ASSERT_FALSE(detector.hasRecentActivity(8.0f, after_rollover));
    
    // Next call should work normally after rollover
    detector.hasRecentActivity(8.0f, 200);
    TEST_ASSERT_TRUE(detector.hasRecentActivity(10.0f, 300)); // Within window now
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_alternator_initial_state);
    RUN_TEST(test_alternator_turn_on);
    RUN_TEST(test_alternator_hysteresis);
    RUN_TEST(test_alternator_reset);
    RUN_TEST(test_activity_first_call);
    RUN_TEST(test_activity_no_change);
    RUN_TEST(test_activity_large_change_within_window);
    RUN_TEST(test_activity_large_change_outside_window);
    RUN_TEST(test_activity_small_change_within_window);
    RUN_TEST(test_activity_threshold_boundary);
    RUN_TEST(test_activity_negative_change);
    RUN_TEST(test_activity_multiple_steps);
    RUN_TEST(test_activity_reset);
    RUN_TEST(test_activity_millis_rollover);
    
    return UNITY_END();
}
