
#include <telemetry_payload.h>

bool buildTelemetryJson(const TelemetryFrame &f, char *out, size_t outLen) {
  // Format temperature
  char tStr[16];
  if (isfinite(f.T)) {
    snprintf(tStr, sizeof(tStr), "%.1f", f.T);
  } else {
    snprintf(tStr, sizeof(tStr), "null");
  }

  // Format Rint
  char rStr[16];
  if (f.hasRint && isfinite(f.Rint_mOhm)) {
    snprintf(rStr, sizeof(rStr), "%.2f", f.Rint_mOhm);
  } else {
    snprintf(rStr, sizeof(rStr), "null");
  }

  // Format Rint25
  char r25Str[16];
  if (f.hasRint25 && isfinite(f.Rint25_mOhm)) {
    snprintf(r25Str, sizeof(r25Str), "%.2f", f.Rint25_mOhm);
  } else {
    snprintf(r25Str, sizeof(r25Str), "null");
  }

  // Format SOH (check for valid value)
  char sohStr[16];
  if (isfinite(f.soh_pct)) {
    snprintf(sohStr, sizeof(sohStr), "%.1f", f.soh_pct);
  } else {
    snprintf(sohStr, sizeof(sohStr), "null");
  }

  int n = snprintf(
      out, outLen,
      "{\"mode\":\"%s\",\"voltage_V\":%.3f,\"current_A\":%.3f,\"temp_C\":%s,"
      "\"soc_pct\":%.1f,\"soh_pct\":%s,\"ah_left\":%.3f,"
      "\"Rint_mOhm\":%s,\"Rint25_mOhm\":%s,\"RintBaseline_mOhm\":%.2f,"
      "\"battery_capacity_ah\":%.1f,"
      "\"alternator_on\":%s,\"rest_s\":%u,\"lowCurrentAccum_s\":%u,"
      "\"hasRint\":%s,\"hasRint25\":%s,\"up_ms\":%lu}",
      f.mode, f.V, f.I, tStr, f.soc_pct, sohStr, f.ah_left, rStr, r25Str,
      f.RintBaseline_mOhm, f.battery_capacity_ah,
      f.alternator_on ? "true" : "false",
      (unsigned)f.rest_s, (unsigned)f.lowCurrentAccum_s,
      f.hasRint ? "true" : "false", f.hasRint25 ? "true" : "false",
      (unsigned long)f.up_ms);

  return n > 0 && (size_t)n < outLen;
}
