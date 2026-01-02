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

- **Battery Configuration** - LTX9-4 Motorcycle Support:
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


## [Unreleased] - 2026-01-02

### Added
- **SET_CAP / SET_BASE** BLE commands: Added textual BLE commands to set runtime configuration:
  - `SET_CAP:12.5`, `SET_CAP 12.5`, `SET_CAP=12.5` — set runtime battery capacity (Ah) and persist to NVS (`battery_capacity_Ah`).
  - `SET_BASE:35.0`, `SET_BASELINE=35.0` — set Rint baseline (mΩ) and persist (`rintBase_mR`).
- **Battery capacity BLE characteristic**: Added `chCapacity` read/notify characteristic exposing the runtime `batteryCapacityAh` value (UUID: `a94f0009-12d3-11ee-be56-0242ac120002`).
- **Runtime persistence API**: `loadBatteryCapacityFromPrefs()` and `setBatteryCapacityAh()` added to `src/app_config.*` to load and persist capacity.
- **Rint learner API**: Added `RintLearner::setBaseline(float mOhm)` and an `extern RintLearner learner` to allow BLE-initiated baseline changes.

### Changed
- **Queued BLE command processing:** BLE write callbacks now enqueue commands only; heavy operations (Preferences clear, restart, NVM writes) are executed in `BleMgr::process()` called from the main loop to avoid blocking NimBLE.
- **Case-insensitive BLE parsing:** Command parsing made case-insensitive so variants like `set_cap` or `Set_Base` are accepted.
- **NVS safety:** Preference reads now check `isKey()` before `getFloat()` to suppress noisy `NOT_FOUND` logs when keys are absent.
- **MQTT notifications:** When capacity or baseline are changed via BLE, the firmware publishes retained JSON messages to the configured `MQTT_TOPIC` so remote listeners receive the latest values immediately.
- **Main loop integration:** `loadBatteryCapacityFromPrefs()` is called from `setup()` and `main.cpp` now calls `ble.process()` after BLE updates so queued commands are executed.

### Fixed
- **NimBLE blocking & stability:** Removed blocking NVM and restart operations from NimBLE callbacks to prevent BLE stack issues.
- **Build errors:** Fixed callback construction compile errors by passing `BleMgr*` into `CommandCallbacks` and updating callback code accordingly.

### Technical Details / Files Modified
- `src/comms/ble_mgr.h` — added `PendingCommand` entries, `enqueueCommand()`, `process()` and `chCapacity` handle.
- `src/comms/ble_mgr.cpp` — parsing for `SET_CAP` / `SET_BASE`, case-insensitive parsing, `CommandCallbacks` now enqueues commands, added `chCapacity` characteristic and notify support, publishes retained MQTT messages on config changes.
- `src/app_config.h` / `src/app_config.cpp` — added `batteryCapacityAh`, `loadBatteryCapacityFromPrefs()`, `setBatteryCapacityAh()` and guarded NVS reads.
- `src/learner/rint_learner.h` — added `setBaseline()` and `extern learner` for external baseline control.
- `src/main.cpp` — call `loadBatteryCapacityFromPrefs()` in `setup()` and `ble.process()` in the main update loop; use runtime `batteryCapacityAh` where applicable.
