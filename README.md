# ESP32 12V Battery Monitor

## 📌 Project Overview
An embedded system for real-time monitoring of a 12V automotive battery using **ESP32** and multiple sensors. The project tracks battery health, estimates State of Charge (SOC) and State of Health (SOH), and provides remote monitoring via MQTT and BLE.

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
- **Runtime persistence:** Settings are persisted to NVS using Preferences. Keys used include `battery_capacity_Ah`, `rintBase_mR` and related Rint values. Reading checks for key existence to avoid errors.
- **MQTT notifications:** When capacity or baseline are changed via BLE the device publishes retained JSON messages to the telemetry `MQTT_TOPIC` so remote systems see the latest values immediately.
- **Case-insensitive parsing:** BLE command parsing is case-insensitive so `set_cap`, `SET_base`, or `Set_Cap` all work.

## 🛠 Hardware Requirements
- ESP32 development board
- INA226 sensor (I²C bus voltage)
- HSTS016L Hall sensor (analog current)
- DS18B20 temperature sensor
- Optional: BLE-enabled smartphone for local monitoring

## **Power: 12V → 3.3V (Automotive)**
Use an automotive-capable DC‑DC step‑down (buck) converter to power the ESP32 from a 12V vehicle battery. Automotive electrical systems present transients, load dumps and wide input voltages.

- **What to look for:** wide input range (eg. ~6–40V or wider), regulated 3.3V output, >=1A (2A recommended), thermal shutdown, short-circuit protection, and low output ripple.
- **Protection & wiring:** place a properly rated fuse near the battery positive, add reverse‑polarity protection (ideal diode or series MOSFET), and a transient voltage suppressor (TVS) or surge protector.
- **Ignition sensing:** if the device should power on/off with vehicle ignition, use an ignition sense line into a GPIO (with proper voltage divider and protection) or control the converter's enable pin.
- **Grounding & EMI:** keep converter and sensor grounds low‑impedance and star‑connected where possible; add common‑mode chokes or ferrite beads if telemetry or sensors show interference.

### Cheap prototyping options
If you want a low-cost way to prototype the 12V → 3.3V power path, these inexpensive modules work fine for bench and short-term in-vehicle testing (not a substitute for an automotive-rated regulator):
- **MP1584EN-based adjustable buck module** — small, adjustable, 3A rated, typical input 4.5–28V.
- **LM2596S adjustable buck module** — very common, inexpensive, input up to ~40V.
- **Note:** Do NOT use linear regulators like `AMS1117-3.3` directly from 12V in a vehicle — they will dissipate too much heat.

Minimum recommended protections and parts for prototyping:
- inline fuse (1–2 A slow blow) near battery +
- reverse polarity protection (ideal diode module or series MOSFET)
- input bulk capacitor (100 µF electrolytic) and 0.1 µF ceramic
- small TVS transient suppressor on the input

Simple wiring example (ASCII):
```
 Battery + ---- Fuse ----+---- VIN+ (buck)
                       |
                       +---- Ignition sense (optional through divider)

 Battery - ------------+---- VIN- (buck) / GND

 VOUT+ (buck 3.3V) -----> 3.3V input on ESP32 (use ESP32 3.3V pin)
 VOUT- (buck GND) ------> ESP32 GND
```
Keep wiring short and use proper gauge for automotive experiments.

## 🔌 Wiring Diagram

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

## 🚀 Build Instructions (PlatformIO)
1. Clone this repository:
   ```bash
   git clone https://github.com/tipih/12VBatteryMonitor.git
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

## **OTA (Over-The-Air) Updates**

This project supports ArduinoOTA for in‑network firmware updates. Below are quick steps to configure and perform OTA uploads from VS Code / PlatformIO.

Prerequisites:
- Device running this firmware and connected to the same LAN as your development PC.
- `ArduinoOTA.begin()` is called in `setup()` (this repository initializes OTA when Wi‑Fi connects).
- If you use an OTA password, set it in `src/secret.h` (see `secrets.example.h`).

PlatformIO configuration (one-time):
- Add an OTA build env using `upload_protocol = espota` (this repo includes `firebeatleV2_ota` in `platformio.ini`).
- Configure `upload_port` to the device IP (or pass it on the CLI). If you use a password, set `upload_flags = --auth=YOUR_OTA_PASSWORD`.

Upload from VS Code (PlatformIO GUI):
- Project Tasks → select `firebeatleV2_ota` → Upload. When prompted, enter the device IP (e.g. 192.168.0.132), or set `upload_port` in `platformio.ini`.

Upload from CLI (copyable):
```bash
pio run -e firebeatleV2_ota -t upload --upload-port 192.168.0.132 --upload-flags="--auth=OTAsecret"
```

If you prefer a keyboard shortcut, create `.vscode/tasks.json` with a task that runs the same `pio` command, then bind a key to run that task in VS Code keyboard shortcuts.

Troubleshooting:
- Ensure your PC and the device are on the same subnet and there is no firewall blocking outgoing TCP to port 3232.
- Check device serial output for OTA logs: the firmware prints `ArduinoOTA initialized` and `OTA Start`/`OTA End` when OTA runs.
- If upload hangs at "Sending invitation", verify the device IP with `ipconfig`/router DHCP page or the retained MQTT `<MQTT_TOPIC>/ip` topic (this firmware publishes the device IP retained).
- If using a password, ensure the same password is passed to PlatformIO via `upload_flags` or defined in `platformio.ini`.

Security note: OTA with a password is better than no auth; for production consider secure network isolation or additional authentication.

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
## 🔗 Home Assistant Integration
<img width="480" height="589" alt="image" src="https://github.com/user-attachments/assets/d3e9bcee-1436-4e59-93e4-8b50853769c7" />

This firmware publishes Home Assistant MQTT discovery payloads (retained) so Home Assistant can auto-discover the device and sensors. Discovery messages are published under topics like:

- `homeassistant/sensor/Toyota_batt_sensor_<sensor>/config`

The following sensors are published and mapped to fields in the JSON telemetry payload:

- `voltage_V` → Battery Voltage (V)
- `current_A` → Battery Current (A)
- `soc_pct` → State of Charge (%)
- `soh_pct` → State of Health (%)
- `Rint_mOhm` / `RintBaseline_mOhm` → Internal resistance (mΩ)
- `ah_left` → Remaining amp-hours (Ah) — formatted to two decimal places for HA display
- `alternator_on` → Alternator state (binary)
- `mode` → `active` / `parked-idle`
- `up_ms` → Uptime (ms)

Notes:

- Discovery payloads are retained so Home Assistant will keep the entity definitions after a restart. The device also republishes discovery messages on MQTT reconnect.
- The `ah_left` sensor is published with two-decimal formatting (e.g. `12.34`) for human-friendly display in Home Assistant.
- If Home Assistant does not auto-discover the device, ensure MQTT integration is configured and that the broker user has permission to publish/subscribe to `homeassistant/#`.


## 📱 BLE Monitoring
- Device name: `ESP32-BattMon`
- Characteristics:
  - Voltage (V)
  - Current (A)
  - Temperature (°C)
  - Mode (Active / Parked-Idle)
- Use any BLE scanner app to view live data.

## 🛠 Hardware Needed
- ESP32 Development Board
<img width="211" height="177" alt="image" src="https://github.com/user-attachments/assets/3e14cbb5-463d-409c-be5f-ce40efd959da" />

- [INA226 Current/Voltage Sensor](https://www.ti.com/product/INA226)
<img width="320" height="472" alt="image" src="https://github.com/user-attachments/assets/87fc7bca-a4c2-4a15-8ee1-bfd22abf9493" />

- HSTS016L Hall Sensor (3.3V variant)
<img width="93" height="135" alt="image" src="https://github.com/user-attachments/assets/50b6d6ef-7d47-49cf-bd10-55df5531f7cf" />
 
- DS18B20 Temperature Sensor
<img width="175" height="223" alt="image" src="https://github.com/user-attachments/assets/77e1a3d9-ac07-467d-9d61-6f986e013ed2" />

- Pull-up resistor for DS18B20 (4.7kΩ)
- Automotive-grade wiring and connectors
- Buck converter for 12V -> 3.3 Volt

## 📄 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
