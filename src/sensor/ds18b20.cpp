
#include "ds18b20.h"

DS18B20Sensor::DS18B20Sensor(int pin, uint8_t resBits)
: _ow(pin), _ds(&_ow), _resBits(resBits) {}

void DS18B20Sensor::begin() {
  _ds.begin(); _ds.setResolution(_resBits);
}

float DS18B20Sensor::readTempC() {
  _ds.requestTemperatures(); 
  float t = _ds.getTempCByIndex(0);
  if (t < -55 || t > 125) return NAN; return t;
}
