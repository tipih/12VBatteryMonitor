#include <cmath>
#include <cstdio>
#include <cstring>
#include <unity.h>

// Mock Arduino dependencies
#define isfinite std::isfinite

// Include telemetry structures and implementation
struct TelemetryFrame {
  const char *mode;
  float V, I, T;
  float soc_pct, soh_pct;
  float Rint_mOhm, Rint25_mOhm, RintBaseline_mOhm;
  float ah_left;
  bool alternator_on;
  uint32_t rest_s, lowCurrentAccum_s, up_ms;
  bool hasRint, hasRint25;
};

bool buildTelemetryJson(const TelemetryFrame &f, char *out, size_t outLen) {
  // Handle all potentially infinite values safely
  char vStr[16], iStr[16], tStr[16], socStr[16], sohStr[16], ahStr[16];

  if (isfinite(f.V)) {
    snprintf(vStr, sizeof(vStr), "%.3f", f.V);
  } else {
    snprintf(vStr, sizeof(vStr), "null");
  }

  if (isfinite(f.I)) {
    snprintf(iStr, sizeof(iStr), "%.3f", f.I);
  } else {
    snprintf(iStr, sizeof(iStr), "null");
  }

  if (isfinite(f.T)) {
    snprintf(tStr, sizeof(tStr), "%.1f", f.T);
  } else {
    snprintf(tStr, sizeof(tStr), "null");
  }

  if (isfinite(f.soc_pct)) {
    snprintf(socStr, sizeof(socStr), "%.1f", f.soc_pct);
  } else {
    snprintf(socStr, sizeof(socStr), "null");
  }

  if (isfinite(f.soh_pct)) {
    snprintf(sohStr, sizeof(sohStr), "%.1f", f.soh_pct);
  } else {
    snprintf(sohStr, sizeof(sohStr), "null");
  }

  if (isfinite(f.ah_left)) {
    snprintf(ahStr, sizeof(ahStr), "%.3f", f.ah_left);
  } else {
    snprintf(ahStr, sizeof(ahStr), "null");
  }

  char rStr[16];
  if (f.hasRint && isfinite(f.Rint_mOhm)) {
    snprintf(rStr, sizeof(rStr), "%.2f", f.Rint_mOhm);
  } else {
    snprintf(rStr, sizeof(rStr), "null");
  }

  char r25Str[16];
  if (f.hasRint25 && isfinite(f.Rint25_mOhm)) {
    snprintf(r25Str, sizeof(r25Str), "%.2f", f.Rint25_mOhm);
  } else {
    snprintf(r25Str, sizeof(r25Str), "null");
  }

  char rBaseStr[16];
  if (isfinite(f.RintBaseline_mOhm)) {
    snprintf(rBaseStr, sizeof(rBaseStr), "%.2f", f.RintBaseline_mOhm);
  } else {
    snprintf(rBaseStr, sizeof(rBaseStr), "null");
  }

  int n = snprintf(
      out, outLen,
      "{\"mode\":\"%s\",\"voltage_V\":%s,\"current_A\":%s,\"temp_C\":%s,"
      "\"soc_pct\":%s,\"soh_pct\":%s,\"ah_left\":%s,"
      "\"Rint_mOhm\":%s,\"Rint25_mOhm\":%s,\"RintBaseline_mOhm\":%s,"
      "\"alternator_on\":%s,\"rest_s\":%u,\"lowCurrentAccum_s\":%u,\"up_ms\":%"
      "lu}",
      f.mode, vStr, iStr, tStr, socStr, sohStr, ahStr, rStr, r25Str, rBaseStr,
      f.alternator_on ? "true" : "false", (unsigned)f.rest_s,
      (unsigned)f.lowCurrentAccum_s, (unsigned long)f.up_ms);

  return n > 0 && (size_t)n < outLen;
}

// Helper function to check if JSON contains a key-value pair
bool jsonContains(const char *json, const char *key, const char *value) {
  char searchStr[256];
  snprintf(searchStr, sizeof(searchStr), "\"%s\":%s", key, value);
  return strstr(json, searchStr) != nullptr;
}

bool jsonContainsString(const char *json, const char *key, const char *value) {
  char searchStr[256];
  snprintf(searchStr, sizeof(searchStr), "\"%s\":\"%s\"", key, value);
  return strstr(json, searchStr) != nullptr;
}

// Test cases
void setUp(void) {}
void tearDown(void) {}

void test_telemetry_basic_active_mode(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.65f,
                          .I = 1.20f,
                          .T = 25.3f,
                          .soc_pct = 85.0f,
                          .soh_pct = 97.5f,
                          .Rint_mOhm = 10.5f,
                          .Rint25_mOhm = 10.2f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 12.340f,
                          .alternator_on = true,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 123456,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContainsString(json, "mode", "active"));
  TEST_ASSERT_TRUE(jsonContains(json, "voltage_V", "12.650"));
  TEST_ASSERT_TRUE(jsonContains(json, "current_A", "1.200"));
  TEST_ASSERT_TRUE(jsonContains(json, "temp_C", "25.3"));
  TEST_ASSERT_TRUE(jsonContains(json, "soc_pct", "85.0"));
  TEST_ASSERT_TRUE(jsonContains(json, "soh_pct", "97.5"));
  TEST_ASSERT_TRUE(jsonContains(json, "alternator_on", "true"));
}

void test_telemetry_parked_idle_mode(void) {
  TelemetryFrame frame = {.mode = "parked-idle",
                          .V = 12.40f,
                          .I = 0.05f,
                          .T = 20.0f,
                          .soc_pct = 75.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 11.0f,
                          .Rint25_mOhm = 10.8f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 10.500f,
                          .alternator_on = false,
                          .rest_s = 300,
                          .lowCurrentAccum_s = 150,
                          .up_ms = 456789,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContainsString(json, "mode", "parked-idle"));
  TEST_ASSERT_TRUE(jsonContains(json, "alternator_on", "false"));
  TEST_ASSERT_TRUE(jsonContains(json, "rest_s", "300"));
  TEST_ASSERT_TRUE(jsonContains(json, "lowCurrentAccum_s", "150"));
}

void test_telemetry_null_temperature(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.50f,
                          .I = 1.0f,
                          .T = NAN,
                          .soc_pct = 80.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 10.0f,
                          .Rint25_mOhm = 10.0f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = true,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "temp_C", "null"));
}

void test_telemetry_null_rint_no_flags(void) {
  TelemetryFrame frame = {
      .mode = "active",
      .V = 12.50f,
      .I = 1.0f,
      .T = 25.0f,
      .soc_pct = 80.0f,
      .soh_pct = 95.0f,
      .Rint_mOhm = 10.5f,
      .Rint25_mOhm = 10.2f,
      .RintBaseline_mOhm = 10.0f,
      .ah_left = 11.0f,
      .alternator_on = false,
      .rest_s = 0,
      .lowCurrentAccum_s = 0,
      .up_ms = 1000,
      .hasRint = false,  // No Rint available
      .hasRint25 = false // No Rint25 available
  };

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "Rint_mOhm", "null"));
  TEST_ASSERT_TRUE(jsonContains(json, "Rint25_mOhm", "null"));
}

void test_telemetry_null_rint_nan_values(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.50f,
                          .I = 1.0f,
                          .T = 25.0f,
                          .soc_pct = 80.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = NAN,
                          .Rint25_mOhm = NAN,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true, // Flag set but value is NAN
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "Rint_mOhm", "null"));
  TEST_ASSERT_TRUE(jsonContains(json, "Rint25_mOhm", "null"));
}

void test_telemetry_null_soh(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.50f,
                          .I = 1.0f,
                          .T = 25.0f,
                          .soc_pct = 80.0f,
                          .soh_pct = NAN, // No SOH available
                          .Rint_mOhm = 10.5f,
                          .Rint25_mOhm = 10.2f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "soh_pct", "null"));
}

void test_telemetry_buffer_too_small(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.50f,
                          .I = 1.0f,
                          .T = 25.0f,
                          .soc_pct = 80.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 10.0f,
                          .Rint25_mOhm = 10.0f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[50]; // Too small
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_FALSE(success);
}

void test_telemetry_exact_buffer_size(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.5f,
                          .I = 1.0f,
                          .T = 25.0f,
                          .soc_pct = 80.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 10.0f,
                          .Rint25_mOhm = 10.0f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true,
                          .hasRint25 = true};

  // First get required size
  char temp[512];
  buildTelemetryJson(frame, temp, sizeof(temp));
  size_t requiredSize = strlen(temp) + 1;

  // Test with exact size
  char *json = new char[requiredSize];
  bool success = buildTelemetryJson(frame, json, requiredSize);

  TEST_ASSERT_TRUE(success);
  delete[] json;
}

void test_telemetry_high_precision_values(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.6789f,
                          .I = 1.2345f,
                          .T = 25.678f,
                          .soc_pct = 85.432f,
                          .soh_pct = 97.654f,
                          .Rint_mOhm = 10.567f,
                          .Rint25_mOhm = 10.234f,
                          .RintBaseline_mOhm = 10.123f,
                          .ah_left = 12.3456f,
                          .alternator_on = true,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 123456,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  // Voltage should be 3 decimal places
  TEST_ASSERT_TRUE(jsonContains(json, "voltage_V", "12.679"));
  // Rint should be 2 decimal places
  TEST_ASSERT_TRUE(jsonContains(json, "Rint_mOhm", "10.57"));
}

void test_telemetry_zero_values(void) {
  TelemetryFrame frame = {.mode = "parked-idle",
                          .V = 0.0f,
                          .I = 0.0f,
                          .T = 0.0f,
                          .soc_pct = 0.0f,
                          .soh_pct = 0.0f,
                          .Rint_mOhm = 0.0f,
                          .Rint25_mOhm = 0.0f,
                          .RintBaseline_mOhm = 0.0f,
                          .ah_left = 0.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 0,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "voltage_V", "0.000"));
  TEST_ASSERT_TRUE(jsonContains(json, "soc_pct", "0.0"));
}

void test_telemetry_large_uptime(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.5f,
                          .I = 1.0f,
                          .T = 25.0f,
                          .soc_pct = 80.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 10.0f,
                          .Rint25_mOhm = 10.0f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 86400,            // 24 hours
                          .lowCurrentAccum_s = 43200, // 12 hours
                          .up_ms = 4294967295,        // Max uint32 (~49 days)
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "rest_s", "86400"));
  TEST_ASSERT_TRUE(jsonContains(json, "up_ms", "4294967295"));
}

void test_telemetry_negative_current_discharge(void) {
  TelemetryFrame frame = {.mode = "active",
                          .V = 12.3f,
                          .I = -5.5f, // Negative = discharge
                          .T = 25.0f,
                          .soc_pct = 60.0f,
                          .soh_pct = 95.0f,
                          .Rint_mOhm = 10.0f,
                          .Rint25_mOhm = 10.0f,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 8.5f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = true,
                          .hasRint25 = true};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "current_A", "-5.500"));
}

void test_telemetry_all_nulls(void) {
  TelemetryFrame frame = {.mode = "parked-sleep",
                          .V = 12.5f,
                          .I = 0.0f,
                          .T = NAN,
                          .soc_pct = 80.0f,
                          .soh_pct = NAN,
                          .Rint_mOhm = NAN,
                          .Rint25_mOhm = NAN,
                          .RintBaseline_mOhm = 10.0f,
                          .ah_left = 11.0f,
                          .alternator_on = false,
                          .rest_s = 0,
                          .lowCurrentAccum_s = 0,
                          .up_ms = 1000,
                          .hasRint = false,
                          .hasRint25 = false};

  char json[512];
  bool success = buildTelemetryJson(frame, json, sizeof(json));

  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "temp_C", "null"));
  TEST_ASSERT_TRUE(jsonContains(json, "soh_pct", "null"));
  TEST_ASSERT_TRUE(jsonContains(json, "Rint_mOhm", "null"));
  TEST_ASSERT_TRUE(jsonContains(json, "Rint25_mOhm", "null"));
}

void test_telemetry_infinite_values(void) {
  TelemetryFrame frame;
  char json[512];

  // Positive infinity voltage (sensor error)
  frame = {.mode = "active",
           .V = INFINITY,
           .I = 2.5f,
           .T = 25.0f,
           .soc_pct = 50.0f,
           .soh_pct = 95.0f,
           .Rint_mOhm = 35.0f,
           .Rint25_mOhm = 36.0f,
           .RintBaseline_mOhm = 35.0f,
           .ah_left = 35.0f,
           .alternator_on = false,
           .rest_s = 0,
           .lowCurrentAccum_s = 0,
           .up_ms = 10000,
           .hasRint = true,
           .hasRint25 = true};
  bool success = buildTelemetryJson(frame, json, sizeof(json));
  TEST_ASSERT_TRUE(success);
  // Infinity should be serialized as null for safety
  TEST_ASSERT_TRUE(jsonContains(json, "voltage_V", "null"));

  // Negative infinity current
  frame.V = 12.5f;
  frame.I = -INFINITY;
  success = buildTelemetryJson(frame, json, sizeof(json));
  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "current_A", "null"));

  // Infinite SOC (calculation error)
  frame.I = 2.5f;
  frame.soc_pct = INFINITY;
  success = buildTelemetryJson(frame, json, sizeof(json));
  TEST_ASSERT_TRUE(success);
  TEST_ASSERT_TRUE(jsonContains(json, "soc_pct", "null"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();

  RUN_TEST(test_telemetry_basic_active_mode);
  RUN_TEST(test_telemetry_parked_idle_mode);
  RUN_TEST(test_telemetry_null_temperature);
  RUN_TEST(test_telemetry_null_rint_no_flags);
  RUN_TEST(test_telemetry_null_rint_nan_values);
  RUN_TEST(test_telemetry_null_soh);
  RUN_TEST(test_telemetry_buffer_too_small);
  RUN_TEST(test_telemetry_exact_buffer_size);
  RUN_TEST(test_telemetry_high_precision_values);
  RUN_TEST(test_telemetry_zero_values);
  RUN_TEST(test_telemetry_large_uptime);
  RUN_TEST(test_telemetry_negative_current_discharge);
  RUN_TEST(test_telemetry_all_nulls);
  RUN_TEST(test_telemetry_infinite_values);

  return UNITY_END();
}
