# ESP32 12V Battery Monitor

## 📌 Project Overview
An embedded system for real-time monitoring of a 12V automotive battery using **ESP32** and multiple sensors. The project tracks battery health, estimates State of Charge (SOC) and State of Health (SOH), and adapts its behavior based on vehicle activity (Active ↔ Parked/Idle ↔ Deep Sleep).

## ✨ Features
- **Voltage, Current, Temperature Monitoring**
- **SOC estimation** via OCV and coulomb counting
- **SOH estimation** using internal resistance (Rint) learning
- Alternator/DC-DC detection for mode switching
- Adaptive telemetry cadence:
  - Active mode: frequent sampling and MQTT publishing
  - Parked/Idle: reduced cadence
  - Deep sleep: wakes every 10 min for snapshot
- **MQTT telemetry** in JSON format for remote monitoring
- **BLE support** for local diagnostics and live data viewing
- Configurable thresholds and timing in `app_config.h`

## **What's New (Jan 2026)**
- **BLE Command API:** New writeable BLE command interface accepts simple textual commands (case-insensitive). Examples:
  - `SET_CAP:12.5` or `SET_CAP 12.5` — set runtime battery capacity (Ah)
  - `SET_BASE:35.0` or `SET_BASELINE=35.0` — set Rint baseline (mΩ)
  - Existing commands (CLEAR, RESET) remain supported.
- **Queued, safe processing:** Commands received over BLE are enqueued and executed in the main loop (avoids blocking the NimBLE task). Call `ble.process()` from your main loop where BLE is updated.
- **Battery capacity exposed via BLE:** A read/notify characteristic (`chCapacity`) exposes the runtime `batteryCapacityAh` value. See [src/comms/ble_mgr.cpp](src/comms/ble_mgr.cpp).
- **Runtime persistence:** Settings are persisted to NVS using Preferences. Keys used include `battery_capacity_Ah`, `rintBase_mR` and related Rint values. Reading checks for key existence to avoid noisy NVS "NOT_FOUND" logs. See [src/app_config.cpp](src/app_config.cpp).
- **MQTT notifications:** When capacity or baseline are changed via BLE the device publishes retained JSON messages to the telemetry `MQTT_TOPIC` so remote systems see the latest values immediately.
- **Case-insensitive parsing:** BLE command parsing is case-insensitive so `set_cap`, `SET_base`, or `Set_Cap` all work.

## 🛠 Hardware Requirements
- ESP32 development board
- INA226 sensor (I²C bus voltage)
- HSTS016L Hall sensor (analog current)
- DS18B20 temperature sensor
- Optional: BLE-enabled smartphone for local monitoring

## 🔌 Wiring Diagram
*(Add your wiring diagram image here)*

## 🚀 Build Instructions (PlatformIO)
1. Clone this repository:
   ```bash
   git clone https://github.com/<youruser>/12VBatteryMonitor.git
   cd 12VBatteryMonitor
   ```
2. Open in VS Code with PlatformIO extension.
3. Install required libraries (PlatformIO will auto-install from `platformio.ini`).
4. Configure Wi-Fi and MQTT credentials:
   - Copy `secrets.example.h` to `secrets.h`
   - Edit `secrets.h` with your credentials.
5. Build and upload:
   ```bash
   pio run --target upload
   ```

## ⚙️ Configuration Guide (`app_config.h`)
Key parameters:
- `BATTERY_CAPACITY_AH` – Battery capacity in Ah
- `INITIAL_BASELINE_mOHM` – Known-good internal resistance baseline
- `ALT_ON_VOLTAGE_V` – Voltage threshold for alternator detection
- Sampling and publishing intervals for Active and Parked modes
- Parked/Idle dwell time and deep sleep intervals

## 📡 MQTT Telemetry Example
```json
{
  "mode": "active",
  "voltage_V": 12.65,
  "current_A": 1.20,
  "temp_C": 25.3,
  "soc_pct": 85.0,
  "soh_pct": 97.5,
  "Rint_mOhm": 10.5,
  "Rint25_mOhm": 10.2,
  "RintBaseline_mOhm": 10.0,
  "alternator_on": true,
  "rest_s": 0,
  "lowCurrentAccum_s": 0,
  "up_ms": 123456,
  "hasRint": true,
  "hasRint25": true
}
```

## 📱 BLE Monitoring
- Device name: `ESP32-BattMon`
- Characteristics:
  - Voltage (V)
  - Current (A)
  - Temperature (°C)
  - Mode (Active / Parked-Idle)
- Use any BLE scanner app to view live data.


## Wiring Diagram

| Component       | ESP32 Pin      |
|-----------------|---------------|
| INA226 SDA      | GPIO21        |
| INA226 SCL      | GPIO22        |
| Hall Sensor Vout| GPIO34 (ADC)  |
| Hall Sensor Vref| GPIO35 (ADC)  |
| DS18B20 Data    | GPIO4         |
| DS18B20 VCC     | 3.3V          |
| DS18B20 GND     | GND           |

**Notes:**
- Add a 4.7kΩ pull-up resistor between DS18B20 Data and 3.3V.
- INA226 uses I²C bus (SDA/SCL).
- Hall sensor requires stable reference voltage (Vref).


## Hardware Needed
- ESP32 Development Board
- [INA226 Current/Voltage Sensor](https://www.ti.com/productor (3.3V variant)
- DS18B20 Temperature Sensor
- Pull-up resistor for DS18B20 (4.7kΩ)
- Automotive-grade wiring and connectors




## 📄 License MIT License

