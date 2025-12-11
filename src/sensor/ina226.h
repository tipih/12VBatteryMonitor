
#pragma once
#include <Arduino.h>

class INA226Bus {
public:
  explicit INA226Bus(uint8_t addr);
  void begin();
  float readBusVoltage_V(); // NAN on failure
private:
  uint8_t _addr; bool readReg16(uint8_t reg, uint16_t& val);
};