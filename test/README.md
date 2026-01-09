# Native Unit Testing

This project includes native unit tests that run on your PC without requiring ESP32 hardware.

## Running Tests

Run all native tests:
```bash
pio test -e native
```

Run with verbose output:
```bash
pio test -e native -v
```

## Test Structure

- `test/test_rint_learner/` - Unit tests for the internal resistance (Rint) learner module
- `test/test_ocv_estimator/` - Unit tests for OCV (Open Circuit Voltage) to SOC estimation
- `test/test_state_detector/` - Unit tests for battery state detection (alternator, activity)
- `test/test_battery_config/` - Unit tests for battery configuration persistence
- `test/test_sleep_mgr/` - Unit tests for power management and deep sleep functionality

## Current Test Coverage

### Rint Learner Tests (`test_rint_learner`) - 5 tests
- **Initialization**: Verifies learner starts with correct baseline and NaN values
- **Set Baseline**: Tests setting and validating baseline values
- **Temperature Compensation**: Validates Rint to Rint25 conversion at different temperatures
- **SOH Calculation**: Tests State of Health calculation based on baseline vs measured resistance
- **SOH with Temperature**: Verifies SOH calculation accounts for temperature-compensated values

### OCV Estimator Tests (`test_ocv_estimator`) - 9 tests
- **Boundary Conditions**: Tests voltage below min and above max SOC points
- **Table Points**: Verifies exact OCV-SOC table values
- **Interpolation**: Tests linear interpolation between table points
- **Temperature Compensation**: Validates voltage compensation to 25°C reference at various temperatures
- **Invalid Input Handling**: Tests behavior with NaN and infinity temperature values
- **Full Workflow**: End-to-end tests with hot and cold battery scenarios

### State Detector Tests (`test_state_detector`) - 13 tests
- **Alternator Detection**: Tests alternator on/off detection with hysteresis
- **Alternator Hysteresis**: Validates hysteresis prevents rapid on/off switching
- **Activity Detection**: Tests current step detection within time window
- **Threshold Boundaries**: Verifies exact threshold behavior
- **Multiple Steps**: Tests consecutive activity events
- **Reset Functionality**: Validates state reset behavior

### Battery Config Tests (`test_battery_config`) - 9 tests
- **Persistence**: Tests loading and saving battery capacity to NVS
- **Primary/Fallback Keys**: Tests dual-key storage strategy
- **Invalid Values**: Verifies rejection of negative, zero, NaN, and infinity values
- **Roundtrip**: Tests full save-load cycle
- **Multiple Updates**: Tests consecutive capacity changes

### Sleep Manager Tests (`test_sleep_mgr`) - 13 tests
- **Preparation & Execution**: Tests two-phase sleep preparation and execution
- **Wireless Shutdown**: Verifies correct shutdown sequence (BLE, WiFi, Bluetooth)
- **Timer Wakeup**: Tests timer-based wakeup interval configuration
- **Time Conversions**: Validates microsecond ↔ minute conversion utilities
- **Multiple Cycles**: Tests consecutive sleep/wake cycles
- **Parameter Verification**: Ensures correct parameters passed to ESP32 functions
- **Edge Cases**: Tests very short and very long sleep intervals

## Adding New Tests

1. Create a new directory under `test/` (e.g., `test/test_module_name/`)
2. Add a test file (e.g., `test_module_name.cpp`)
3. Include Unity test framework: `#include <unity.h>`
4. Write test functions starting with `test_`
5. Add `RUN_TEST(test_function_name)` in `main()`
6. Run tests with `pio test -e native`

## Test Framework

Tests use the [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity) which provides:
- `TEST_ASSERT_EQUAL_FLOAT(expected, actual)`
- `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)`
- `TEST_ASSERT_TRUE(condition)`
- `TEST_ASSERT_FALSE(condition)`
- And many more assertions

## Mock Dependencies

The test files include mock implementations of ESP32 and Arduino-specific functions:
- `millis()` - Returns a controllable mock time value
- `Preferences` - Mock NVS storage class
- `DebugPublisher` - Mock debug output
- **ESP32 Sleep Functions** - Mock `esp_sleep_enable_timer_wakeup()`, `esp_deep_sleep_start()`
- **Wireless Functions** - Mock WiFi, BLE (NimBLE), and Bluetooth Classic shutdown
- **Call Tracking** - Records all mock function calls for verification in tests

## Next Steps

Consider adding tests for:
- `telemetry_payload` - Telemetry data formatting
- Additional edge cases for existing modules
- Integration tests combining multiple modules
