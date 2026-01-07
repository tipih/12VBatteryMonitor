// Simple battery capacity persistence helper
#pragma once
#include <Arduino.h>

class BatteryConfig {
public:
    void begin();
    void setCapacity(float ah);
};

extern BatteryConfig batteryConfig;
