#include "state_detector.h"

bool BatteryStateDetector::alternatorOn(float V) {
  const float ON_THRESH = ALT_ON_VOLTAGE_V;
  const float OFF_THRESH = ON_THRESH - 0.25f;

  if (_altState)
    _altState = (V >= OFF_THRESH); // hysteresis: stay on until drops below OFF
  else
    _altState = (V >= ON_THRESH); // hysteresis: turn on at ON threshold

  return _altState;
}

bool BatteryStateDetector::hasRecentActivity(float currentA, uint32_t nowMs) {
  if (!isfinite(_prevCurrent)) {
    _prevCurrent = currentA;
    _prevMs = nowMs;
    return false;
  }

  float dI = fabsf(currentA - _prevCurrent);
  uint32_t dt = nowMs - _prevMs;
  bool active = (dI >= STEP_ACTIVITY_DI_A && dt <= STEP_ACTIVITY_WINDOW_MS);

  _prevCurrent = currentA;
  _prevMs = nowMs;

  return active;
}

void BatteryStateDetector::reset() {
  _altState = false;
  _prevCurrent = NAN;
  _prevMs = 0;
}