
#pragma once
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class DS18B20Sensor {
public:
  DS18B20Sensor(int pin, uint8_t resBits);
  void  begin();
  float readTempC(); // NAN if out-of-range
private:
  OneWire _ow; DallasTemperature _ds; uint8_t _resBits;
};