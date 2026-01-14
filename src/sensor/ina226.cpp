#include "ina226.h"
#include <Wire.h>

INA226Bus::INA226Bus(uint8_t addr) : _addr(addr) {}
void INA226Bus::begin() { Wire.begin(); }

bool INA226Bus::readReg16(uint8_t reg, uint16_t &val) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom((int)_addr, 2) != 2)
    return false;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  val = (uint16_t(msb) << 8) | lsb;
  return true;
}

float INA226Bus::readBusVoltage_V() {
  uint16_t raw = 0;
  if (!readReg16(0x02, raw))
    return NAN;
  return raw * 0.00125f;
}