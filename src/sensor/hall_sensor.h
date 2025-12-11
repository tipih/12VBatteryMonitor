#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct HallZeroStore {
  float zero_mV = NAN;
  bool  load();
  void  save(float z);
};

struct HallJitterStats { float min_mV{0}, max_mV{0}; };

class HallSensor {
public:
  HallSensor(int pinVout, int pinVref, bool haveVref,
             int adcBits, adc_attenuation_t atten,
             float sensorRatingA, int sign);
  void begin();
  float readCurrentA(uint8_t samples, HallJitterStats* js = nullptr);
  float captureZeroTrimmedMean(int N);
  float zero_mV() const { return _zero_mV; }
  void  setZero(float z) { _zero_mV = z; }

private:
  int   _pinVout, _pinVref; bool  _haveVref; int   _adcBits; adc_attenuation_t _atten; float _sensorRatingA; int   _sign; bool  _configured{false}; float _zero_mV{0.0f};
  static int   readMilliVoltsMedian5(int pin);
  void         readDeltaBlock_mV(int N, float* deltas_out, float* vout0_mV, float* vref0_mV);
  float        readVref_mV_avg(int N);
};