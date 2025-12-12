#pragma once
#include <Arduino.h>
#include <app_config.h>

class BatteryStateDetector {
public:
    // Alternator detection with hysteresis
    bool alternatorOn(float V);

    // Step activity detection
    bool hasRecentActivity(float currentA, uint32_t nowMs);

    void reset();

private:
    // Hysteresis state
    bool _altState = false;

    // Activity tracking
    float _prevCurrent = NAN;
    uint32_t _prevMs = 0;
};