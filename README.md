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

## **Power: 12V → 3.3V (Automotive)**

Use an automotive-capable DC‑DC step‑down (buck) converter to power the ESP32 from a 12V vehicle battery. Automotive electrical systems present transients, load dumps and wide input voltages, so choose or protect your regulator accordingly:

- **What to look for:** wide input range (eg. ~6–40V or wider), regulated 3.3V output, >=1A (2A recommended), thermal shutdown, short-circuit protection, and low output ripple.
- **Protection & wiring:** place a properly rated fuse near the battery positive, add reverse‑polarity protection (ideal diode or series MOSFET), and a transient voltage suppressor (TVS) or surge protector on the input. Use bulk input capacitance and an LC/RFI filter to reduce noise.
- **Ignition sensing:** if the device should power on/off with vehicle ignition, use an ignition sense line into a GPIO (with proper voltage divider and protection) or control the converter's enable pin using an ignition‑sensing circuit.
- **Grounding & EMI:** keep converter and sensor grounds low‑impedance and star‑connected where possible; add common‑mode chokes or ferrite beads if telemetry or sensors show interference.

If you need a simple bench option for development, use a high quality wide‑input buck module, but for production prefer an automotive‑rated DC‑DC module or an inline protection board designed for automotive use.

### Cheap prototyping options
If you want a low-cost way to prototype the 12V → 3.3V power path, these inexpensive modules work fine for bench and short-term in-vehicle testing (not a substitute for an automotive-rated regulator in production):

- **MP1584EN-based adjustable buck module** (often sold as "MP1584" or "XlSemi MP1584") — small, adjustable, 3A rated, typical input 4.5–28V. Good balance of cost and capability.
- **LM2596S adjustable buck module** — very common, inexpensive, input often up to ~40V, but larger and with higher ripple; OK for prototyping.
- **Note:** Do NOT use linear regulators like `AMS1117-3.3` directly from 12V in a vehicle — they will dissipate too much heat.

Minimum recommended protections and parts for prototyping:
- inline fuse (1–2 A slow blow) near battery +
- reverse polarity protection (ideal diode module or series MOSFET)
- input bulk capacitor (100 µF electrolytic) and 0.1 µF ceramic
- small TVS transient suppressor (automotive TVS or similar) on the input

Simple wiring example (ASCII):

 Battery + ---- Fuse ----+---- VIN+ (buck)
                       |
                       +---- Ignition sense (optional through divider)

 Battery - ------------+---- VIN- (buck) / GND

 VOUT+ (buck 3.3V) -----> 3.3V input on ESP32 (use ESP32 3.3V pin)
 VOUT- (buck GND) ------> ESP32 GND

Keep wiring short and use proper gauge for automotive experiments. For any permanent or long-term in-vehicle installation, choose an automotive-rated DC-DC converter and add proper surge protection and isolation.

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

