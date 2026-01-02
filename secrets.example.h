// secrets.example.h
// Copy this file to src/secret.h (or to secrets.h if your build expects that)
// and fill in real values. Do NOT commit `src/secret.h` with real credentials.

#pragma once

// Wi‑Fi
constexpr const char* WIFI_SSID     = "YOUR_WIFI_SSID";
constexpr const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT
constexpr const char* MQTT_HOST     = "mqtt.example.com"; // hostname or IP
constexpr const char* MQTT_PORT     = "1883"; // numeric port as string or change to int in your code
constexpr const char* MQTT_USER     = "mqtt_user";
constexpr const char* MQTT_PASS     = "mqtt_password";
constexpr const char* MQTT_TOPIC    = "battmon/telemetry"; // telemetry topic

// BLE device name (optional)
constexpr const char* BLE_DEVICE_NAME = "ESP32-BattMon";

// Optional: Over-the-air update password / admin token
// constexpr const char* OTA_PASSWORD = "your_ota_password";

// Example: copy to src/secret.h and then edit values.
