# Changelog

All notable changes to the 12V Battery Monitor project.

## [Unreleased] - 2025-12-14

### Added
- **BLE Command Interface**: Added command characteristic for remote device control via Bluetooth
  - `CLEAR_NVM` command: Clears all non-volatile memory (battmon and hall namespaces)
  - `RESET` command: Restarts the device
  - `CLEAR_RESET` command: Clears NVM then restarts (factory reset)
  - Command characteristic UUID: `a94f0006-12d3-11ee-be56-0242ac120002`
  - Command callbacks with response notifications (`NVM_CLEARED`, `RESETTING`, etc.)
  - Serial logging for all command operations

### Changed
- **Deep Sleep Timing** - Optimized for production use:
  - `PARKED_WAKE_INTERVAL_US`: Changed from 3 minutes to **5 minutes** (300 seconds)
  - `PARKED_IDLE_MAX_MS`: Changed from 60 minutes to **20 minutes** before deep sleep
  - `PARKED_IDLE_ENTRY_DWELL_SEC`: Changed from 5 minutes to **2 minutes** to enter idle state

- **Battery Configuration** - LTX9-4 Motorcycle Battery Support:
  - `BATTERY_CAPACITY_AH`: Changed from 70.0Ah to **8.0Ah** (later adjusted to 9.0Ah)
  - `INITIAL_BASELINE_mOHM`: Changed from 10.0mΩ to **35.0mΩ** (correct baseline for LTX9-4)

- **Rint Learner Improvements** - Enhanced step detection reliability:
  - `STEP_WINDOW_MS`: Increased from 120ms to **1000ms** (2x sampling interval for better detection)
  - `SCAN_BACK_MS`: Increased from 800ms to **2000ms** (extended lookback window)
  - Minimum sample requirement: Reduced from 4 to **3 samples** per window (line 222)
  - Better accommodates 500ms sampling rate in active mode

### Fixed
- **SOH Calculation**: Corrected unrealistic State of Health readings
  - Previous: 21-31% (due to incorrect 10mΩ baseline for 8Ah battery)
  - Current: 75-100% (with proper 35mΩ baseline for LTX9-4)
  
- **Rint Step Detection**: Resolved "step_rejected: few_samples" issues
  - Widened detection window to match actual sampling rate
  - Reduced minimum sample requirements for faster detection
  - Improved stability with load step transitions

### Technical Details
- **Files Modified**:
  - `src/app_config.h`: Battery capacity and baseline configuration
  - `src/learner/rint_learner.h`: Step detection timing parameters
  - `src/comms/ble_mgr.h`: Added command characteristic handle
  - `src/comms/ble_mgr.cpp`: Implemented CommandCallbacks class with NVM clear and reset functionality
  - `src/main.cpp`: Added debug output for Rint calculations (lines 312-318)

### Notes
- BLE commands can be sent via nRF Connect or LightBlue apps
- HEX format commands:
  - `CLEAR_NVM`: `434C4541525F4E564D`
  - `RESET`: `5245534554`
  - `CLEAR_RESET`: `434C4541525F5245534554`
- Factory reset clears learned Rint baseline and Hall sensor calibration
- New baseline will be established after 10+ minutes of load step observations
