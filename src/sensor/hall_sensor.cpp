
#include "hall_sensor.h"
#include <algorithm>

static inline void shortDelay() { delayMicroseconds(80); }

HallSensor::HallSensor(int pinVout, int pinVref, bool haveVref,
  int adcBits, adc_attenuation_t atten,
  float sensorRatingA, int sign)
  : _pinVout(pinVout), _pinVref(pinVref), _haveVref(haveVref),
  _adcBits(adcBits), _atten(atten),
  _sensorRatingA(sensorRatingA), _sign(sign) {
}

void HallSensor::begin() {
  if (_configured) return;
  analogReadResolution(_adcBits);
  analogSetPinAttenuation(_pinVout, _atten);
  if (_haveVref) analogSetPinAttenuation(_pinVref, _atten);
  _configured = true;
}

int HallSensor::readMilliVoltsMedian5(int pin) {
  int v[5];
  for (int i = 0;i < 5;++i) v[i] = analogReadMilliVolts(pin);
  for (int i = 1;i < 5;++i) {
    int key = v[i], j = i - 1;
    while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; --j; }
    v[j + 1] = key;
  }
  return v[2];
}

void HallSensor::readDeltaBlock_mV(int N, float* deltas_out,
  float* vout0_mV, float* vref0_mV) {
  float vout = readMilliVoltsMedian5(_pinVout);
  float vref = _haveVref ? readMilliVoltsMedian5(_pinVref) : vout;
  if (vout0_mV) *vout0_mV = vout;
  if (vref0_mV) *vref0_mV = vref;
  if (deltas_out) deltas_out[0] = vout - vref;
  for (int i = 1;i < N;++i) {
    float vo = readMilliVoltsMedian5(_pinVout);
    float vr = _haveVref ? readMilliVoltsMedian5(_pinVref) : vo;
    if (deltas_out) deltas_out[i] = vo - vr;
    shortDelay();
  }
}

float HallSensor::readVref_mV_avg(int N) {
  long acc = 0;
  if (!_haveVref) {
    for (int i = 0;i < N;++i) { acc += readMilliVoltsMedian5(_pinVout); shortDelay(); }
    return acc / (float)N;
  }
  for (int i = 0;i < N;++i) { acc += readMilliVoltsMedian5(_pinVref); shortDelay(); }
  return acc / (float)N;
}

float HallSensor::readCurrentA(uint8_t samples, HallJitterStats* js) {
  begin();
  int N = (samples <= 0 ? 1 : (samples > 64 ? 64 : samples));
  float delta_buf[64]; float vout_once = 0, vref_once = 0;
  readDeltaBlock_mV(N, delta_buf, &vout_once, &vref_once);

  double acc = 0.0; for (int i = 0;i < N;++i) acc += delta_buf[i];
  const float delta_raw_mV = (float)(acc / N);
  float delta_corr_mV = delta_raw_mV - _zero_mV;

  const float vref_avg_mV = readVref_mV_avg(16);
  const float vcc_V = (2.0f * vref_avg_mV) / 1000.0f;
  const float mV_per_A = (625.0f * (vcc_V / 5.0f)) / _sensorRatingA;

  const float DEADBAND_mV = 1.5f;
  if (fabsf(delta_corr_mV) < DEADBAND_mV) delta_corr_mV = 0.0f;

  float minD = delta_buf[0], maxD = delta_buf[0];
  for (int i = 1;i < N;++i) { if (delta_buf[i] < minD) minD = delta_buf[i]; if (delta_buf[i] > maxD) maxD = delta_buf[i]; }
  if (js) { js->min_mV = minD; js->max_mV = maxD; }

  const float current_A = (delta_corr_mV / mV_per_A) * _sign;

#ifdef DEBUG_HALL_SENSOR
  Serial.printf(
    u8"Vref=%.1f mV Vout=%.1f mV Δraw=%.2f mV zero=%.2f mV Δcorr=%.2f mV "
    "mV/A=%.3f I=%.3f A Δrange=[%.2f..%.2f] mV\n",
    vref_once, vout_once, delta_raw_mV, _zero_mV, delta_corr_mV,
    mV_per_A, current_A, minD, maxD
  );
#endif

  return current_A;
}

float HallSensor::captureZeroTrimmedMean(int N) {
  if (N < 16) N = 16; if (N > 256) N = 256;
  float vout0 = 0, vref0 = 0; float delta_buf[256];
  readDeltaBlock_mV(N, delta_buf, &vout0, &vref0);
  std::sort(delta_buf, delta_buf + N);
  int start = N / 10; int end = N - start; double acc = 0.0;
  for (int i = start;i < end;++i) acc += delta_buf[i];
  float newZero = (float)(acc / (end - start));
  _zero_mV = newZero;

  Serial.printf(
    u8"HALL zero captured: %.3f mV (from %d) FirstPair: Vref=%.1f mV Vout=%.1f mV Δ=%.2f mV\n",
    _zero_mV, N, vref0, vout0, (vout0 - vref0)
  );

  return _zero_mV;
}

// NVS helpers
bool HallZeroStore::load() {
  Preferences p; p.begin("hall", true);
  float z = p.getFloat("zero_mV", NAN);
  p.end();
  if (isfinite(z)) { zero_mV = z; return true; }
  return false;
}
void HallZeroStore::save(float z) {
  Preferences p; p.begin("hall", false);
  p.putFloat("zero_mV", z);
  p.end();
  zero_mV = z;
  Serial.printf("HALL zero saved: %.3f mV", z);
}
