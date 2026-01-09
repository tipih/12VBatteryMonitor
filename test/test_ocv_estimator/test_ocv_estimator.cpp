#include <unity.h>
#include <cmath>

// Mock Arduino dependencies
#define isfinite std::isfinite

// Include the OCV estimator implementation inline for testing
class OcvEstimator {
public:
    static float socFromOCV(float voltage_V);
    static float compensateTo25C(float vbatt, float tempC);

private:
    struct OcvPoint { float v; float soc; };
    static constexpr OcvPoint OCV_TABLE[] = {
      {11.90f, 5.0f}, {12.00f, 15.0f}, {12.20f, 50.0f},
      {12.40f, 75.0f}, {12.60f, 95.0f}, {12.72f, 100.0f}
    };
    static constexpr int TABLE_SIZE = 6;
};

constexpr OcvEstimator::OcvPoint OcvEstimator::OCV_TABLE[];

float OcvEstimator::socFromOCV(float voltage_V) {
    if (voltage_V <= OCV_TABLE[0].v) return OCV_TABLE[0].soc;

    for (int i = 1; i < OcvEstimator::TABLE_SIZE; ++i) {
        if (voltage_V <= OCV_TABLE[i].v) {
            float t = (voltage_V - OCV_TABLE[i - 1].v) /
                (OCV_TABLE[i].v - OCV_TABLE[i - 1].v);
            return OCV_TABLE[i - 1].soc + t * (OCV_TABLE[i].soc - OCV_TABLE[i - 1].soc);
        }
    }
    return 100.0f;
}

float OcvEstimator::compensateTo25C(float vbatt, float tempC) {
    if (!isfinite(tempC)) return vbatt;
    float dv = -0.018f * (tempC - 25.0f);  // ~-18 mV/°C
    return vbatt + dv;
}

// Test cases
void setUp(void) {}
void tearDown(void) {}

void test_ocv_soc_boundary_conditions(void) {
    // Below minimum voltage
    TEST_ASSERT_EQUAL_FLOAT(5.0f, OcvEstimator::socFromOCV(11.50f));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, OcvEstimator::socFromOCV(11.90f));
    
    // Above maximum voltage
    TEST_ASSERT_EQUAL_FLOAT(100.0f, OcvEstimator::socFromOCV(12.72f));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, OcvEstimator::socFromOCV(13.00f));
}

void test_ocv_soc_table_points(void) {
    // Test exact table points
    TEST_ASSERT_EQUAL_FLOAT(5.0f, OcvEstimator::socFromOCV(11.90f));
    TEST_ASSERT_EQUAL_FLOAT(15.0f, OcvEstimator::socFromOCV(12.00f));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, OcvEstimator::socFromOCV(12.20f));
    TEST_ASSERT_EQUAL_FLOAT(75.0f, OcvEstimator::socFromOCV(12.40f));
    TEST_ASSERT_EQUAL_FLOAT(95.0f, OcvEstimator::socFromOCV(12.60f));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, OcvEstimator::socFromOCV(12.72f));
}

void test_ocv_soc_interpolation(void) {
    // Test midpoint between 12.00V (15%) and 12.20V (50%)
    // At 12.10V: should be ~32.5%
    float soc = OcvEstimator::socFromOCV(12.10f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 32.5f, soc);
    
    // Test 25% along from 12.20V (50%) to 12.40V (75%)
    // At 12.25V: should be ~56.25%
    soc = OcvEstimator::socFromOCV(12.25f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 56.25f, soc);
    
    // Test 75% along from 12.40V (75%) to 12.60V (95%)
    // At 12.55V: should be ~90%
    soc = OcvEstimator::socFromOCV(12.55f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, soc);
}

void test_temperature_compensation_at_reference(void) {
    // At 25°C, voltage should not change
    TEST_ASSERT_EQUAL_FLOAT(12.50f, OcvEstimator::compensateTo25C(12.50f, 25.0f));
}

void test_temperature_compensation_hot(void) {
    // At 35°C (10° above reference), voltage should decrease by ~0.18V
    // Compensated voltage = actual + 0.18 = 12.50 + 0.18 = 12.68V
    float compensated = OcvEstimator::compensateTo25C(12.50f, 35.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.32f, compensated);
}

void test_temperature_compensation_cold(void) {
    // At 15°C (10° below reference), voltage should increase by ~0.18V
    // Compensated voltage = actual - 0.18 = 12.50 - 0.18 = 12.32V
    float compensated = OcvEstimator::compensateTo25C(12.50f, 15.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.68f, compensated);
}

void test_temperature_compensation_invalid(void) {
    // With invalid temperature, voltage should return unchanged
    TEST_ASSERT_EQUAL_FLOAT(12.50f, OcvEstimator::compensateTo25C(12.50f, NAN));
    TEST_ASSERT_EQUAL_FLOAT(12.50f, OcvEstimator::compensateTo25C(12.50f, INFINITY));
}

void test_full_workflow_hot_battery(void) {
    // Scenario: Battery at 30°C reads 12.30V
    // Compensate to 25°C: 12.30 + (-0.018 * 5) = 12.30 - 0.09 = 12.21V
    float compensated = OcvEstimator::compensateTo25C(12.30f, 30.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.21f, compensated);
    
    // Get SOC from compensated voltage (should be ~50.5%)
    float soc = OcvEstimator::socFromOCV(compensated);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 51.0f, soc);
}

void test_full_workflow_cold_battery(void) {
    // Scenario: Battery at 10°C reads 12.40V
    // Compensate to 25°C: 12.40 + (-0.018 * -15) = 12.40 + 0.27 = 12.67V
    float compensated = OcvEstimator::compensateTo25C(12.40f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.67f, compensated);
    
    // Get SOC from compensated voltage (should be ~98%)
    float soc = OcvEstimator::socFromOCV(compensated);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 98.0f, soc);
}

void test_extreme_voltage_values(void) {
    OcvEstimator estimator;
    
    // Voltage way below table minimum (possibly reversed polarity or dead battery)
    float soc = estimator.socFromOCV(5.0f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, soc); // Clamps to table minimum (5%)
    
    // Voltage way above table maximum (overcharge or measurement error)
    soc = estimator.socFromOCV(16.0f);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, soc); // Should clamp to 100%
    
    // Infinite voltage (sensor error)
    soc = estimator.socFromOCV(INFINITY);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, soc); // Treat as max
    
    // Negative voltage
    soc = estimator.socFromOCV(-12.0f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, soc); // Clamps to table minimum
}

void test_extreme_temperature_values(void) {
    OcvEstimator estimator;
    
    // Arctic conditions (-40°C)
    // Temp compensation: 12.50 + (25 - (-40)) * 0.018 = 12.50 + 1.17 = 13.67V
    float v25 = estimator.compensateTo25C(12.50f, -40.0f);
    float soc = estimator.socFromOCV(v25);
    TEST_ASSERT_TRUE(soc >= 0.0f && soc <= 100.0f);
    
    // Desert conditions (60°C)
    // Temp compensation: 12.50 + (25 - 60) * 0.018 = 12.50 - 0.63 = 11.87V
    v25 = estimator.compensateTo25C(12.50f, 60.0f);
    soc = estimator.socFromOCV(v25);
    TEST_ASSERT_TRUE(soc >= 0.0f && soc <= 100.0f);
    
    // Sensor failure (infinite temperature) - should return original voltage
    v25 = estimator.compensateTo25C(12.50f, INFINITY);
    TEST_ASSERT_EQUAL_FLOAT(12.50f, v25); // No compensation
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_ocv_soc_boundary_conditions);
    RUN_TEST(test_ocv_soc_table_points);
    RUN_TEST(test_ocv_soc_interpolation);
    RUN_TEST(test_temperature_compensation_at_reference);
    RUN_TEST(test_temperature_compensation_hot);
    RUN_TEST(test_temperature_compensation_cold);
    RUN_TEST(test_temperature_compensation_invalid);
    RUN_TEST(test_full_workflow_hot_battery);
    RUN_TEST(test_full_workflow_cold_battery);
    RUN_TEST(test_extreme_voltage_values);
    RUN_TEST(test_extreme_temperature_values);
    
    return UNITY_END();
}
