#pragma once
#include <Arduino.h>
#include <app_config.h>

class OcvEstimator {
public:
    // Get SOC from OCV at 25°C
    static float socFromOCV(float voltage_V);

    // Compensate voltage to 25°C reference
    static float compensateTo25C(float vbatt, float tempC);

private:
    struct OcvPoint { float v; float soc; };
    static constexpr OcvPoint OCV_TABLE[] = {
      {11.90f, 5.0f}, {12.00f, 15.0f}, {12.20f, 50.0f},
      {12.40f, 75.0f}, {12.60f, 95.0f}, {12.72f, 100.0f}
    };
    static constexpr int TABLE_SIZE = 6;
};