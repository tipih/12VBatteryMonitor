#include "ocv_estimator.h"

constexpr OcvEstimator::OcvPoint OcvEstimator::OCV_TABLE[];

float OcvEstimator::socFromOCV(float voltage_V) {
  if (voltage_V <= OCV_TABLE[0].v)
    return OCV_TABLE[0].soc;

  for (int i = 1; i < OcvEstimator::TABLE_SIZE; ++i) {
    if (voltage_V <= OCV_TABLE[i].v) {
      float t = (voltage_V - OCV_TABLE[i - 1].v) /
                (OCV_TABLE[i].v - OCV_TABLE[i - 1].v);
      return OCV_TABLE[i - 1].soc +
             t * (OCV_TABLE[i].soc - OCV_TABLE[i - 1].soc);
    }
  }
  return 100.0f;
}

float OcvEstimator::compensateTo25C(float vbatt, float tempC) {
  if (!isfinite(tempC))
    return vbatt;
  float dv = -0.018f * (tempC - 25.0f); // ~-18 mV/°C
  return vbatt + dv;
}