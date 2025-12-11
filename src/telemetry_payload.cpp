
#include <telemetry_payload.h>

bool buildTelemetryJson(const TelemetryFrame& f, char* out, size_t outLen) {
  const char* tStr = isfinite(f.T) ? String(f.T,1).c_str() : "null";
  const char* rStr = (f.hasRint && isfinite(f.Rint_mOhm)) ? String(f.Rint_mOhm,2).c_str() : "null";
  const char* r25Str= (f.hasRint25 && isfinite(f.Rint25_mOhm)) ? String(f.Rint25_mOhm,2).c_str() : "null";

  
int n = snprintf(
  out, outLen,
  "{\"mode\":\"%s\",\"voltage_V\":%.3f,\"current_A\":%.3f,\"temp_C\":%s,"
  "\"soc_pct\":%.1f,\"soh_pct\":%.1f,"
  "\"Rint_mOhm\":%s,\"Rint25_mOhm\":%s,\"RintBaseline_mOhm\":%.2f,"
  "\"alternator_on\":%s,\"rest_s\":%u,\"lowCurrentAccum_s\":%u,\"up_ms\":%lu}",
  f.mode, f.V, f.I, tStr,
  f.soc_pct, f.soh_pct,
  rStr, r25Str, f.RintBaseline_mOhm,
  f.alternator_on ? "true" : "false",
  (unsigned)f.rest_s, (unsigned)f.lowCurrentAccum_s, (unsigned long)f.up_ms
);

  return n > 0 && (size_t)n < outLen;
}
