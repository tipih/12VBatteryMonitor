#pragma once
#include "../app_config.h"
#include "../comms/debug_publisher.h"
#include <Arduino.h>
#include <Preferences.h>
#include <algorithm>

struct Sample {
  float V;
  float I;
  float T;
  uint32_t t;
};

class RintLearner {
public:
  void begin(float initialBaseline_mOhm, DebugPublisher *dbg = nullptr) {
    _dbg = dbg;
    prefs.begin("battmon", false);
    _baseline_mOhm = prefs.getFloat("rintBase_mR", -1.0f);
    if (!isfinite(_baseline_mOhm) || _baseline_mOhm <= 0.5f ||
        _baseline_mOhm > 500.0f) {
      _baseline_mOhm = initialBaseline_mOhm;
      prefs.putFloat("rintBase_mR", _baseline_mOhm);
    }
    // Load last measured Rint values from NVM with validation
    _lastRint_mOhm = prefs.getFloat("lastRint_mR", NAN);
    _lastRint25_mOhm = prefs.getFloat("lastR25_mR", NAN);

    // Validate loaded Rint25 value: if out of range, reset to avoid SOH
    // corruption
    bool rint25Valid = true;
    if (!isfinite(_lastRint25_mOhm) || _lastRint25_mOhm < MIN_RINT25_mOHM ||
        _lastRint25_mOhm > MAX_RINT25_mOHM) {
      rint25Valid = false;
    }
    // Also check if it's below or above baseline ratio (physically implausible)
    if (rint25Valid && _lastRint25_mOhm < _baseline_mOhm * MIN_RINT25_VS_BASELINE_RATIO) {
      rint25Valid = false;
    }
    if (rint25Valid && _lastRint25_mOhm > _baseline_mOhm * MAX_RINT25_VS_BASELINE_RATIO) {
      rint25Valid = false;
    }
    
    if (!rint25Valid) {
      _lastRint25_mOhm = NAN;
      _lastRint_mOhm = NAN;
      prefs.putFloat("lastRint_mR", NAN);
      prefs.putFloat("lastR25_mR", NAN);
      if (_dbg && _dbg->ok()) {
        _dbg->send(R"({"event":"rint25_invalid_reset"})", "init");
      }
    }
    _lastBaselineUpdateMs = millis();
  }

  void ingest(float V, float I, float T, uint32_t nowMs) {
    push({V, I, T, nowMs});
    tryDetectAndLearn(nowMs);
  }

  float baseline_mOhm() const { return _baseline_mOhm; }
  float lastRint_mOhm() const { return _lastRint_mOhm; }
  float lastRint25_mOhm() const { return _lastRint25_mOhm; }

  // Manually set the baseline (mOhm) and persist immediately.
  void setBaseline(float baseline_mOhm) {
    if (!isfinite(baseline_mOhm) || baseline_mOhm <= 0.1f ||
        baseline_mOhm > 1000.0f)
      return;
    _baseline_mOhm = baseline_mOhm;
    prefs.begin("battmon", false);
    prefs.putFloat("rintBase_mR", _baseline_mOhm);
    prefs.end();
    _lastBaselineUpdateMs = millis();
    if (_dbg && _dbg->ok()) {
      char js[128];
      snprintf(js, sizeof(js), R"({"event":"baseline_set","value":%.3f})",
               _baseline_mOhm);
      _dbg->send(js, "baseline_set");
    }
  }

  float currentSOH() const {
    // Validate Rint25: reject if outside absolute bounds or baseline ratios
    if (!isfinite(_lastRint25_mOhm) || _lastRint25_mOhm < MIN_RINT25_mOHM ||
        _lastRint25_mOhm > MAX_RINT25_mOHM)
      return 1.0f;
    if (_lastRint25_mOhm < _baseline_mOhm * MIN_RINT25_VS_BASELINE_RATIO ||
        _lastRint25_mOhm > _baseline_mOhm * MAX_RINT25_VS_BASELINE_RATIO)
      return 1.0f;
    float soh = _baseline_mOhm / _lastRint25_mOhm;
    if (soh < 0.0f)
      soh = 0.0f;
    if (soh > 1.0f)
      soh = 1.0f;
    return soh;
  }

private:
  // ---- Constants ----
  static constexpr int RB_CAPACITY = 512;
  static constexpr float I_STEP_MIN_A = 1.8f;
  static constexpr float I_ABS_MIN_A = 2.0f;
  static constexpr uint32_t STEP_WINDOW_MS = 1000; // Widened for 500ms sampling
  static constexpr uint32_t STEP_SEPARATION_MS = 100;
  static constexpr uint32_t SCAN_BACK_MS = 2000; // Extended lookback window
  static constexpr float MAX_EVENT_dV_V = 0.90f;
  static constexpr float MAX_EVENT_dI_A = 60.0f;
  static constexpr float EWMA_UP_ALPHA = 0.10f;
  static constexpr float EWMA_DOWN_ALPHA = 0.02f;
  static constexpr float BASELINE_DEADBAND_mOHM = 0.2f;
  static constexpr float MAX_RISE_PER_HR = 0.02f;
  static constexpr float MAX_FALL_PER_HR = 0.004f;
  static constexpr uint32_t MIN_UPDATE_INTERVAL_MS = 10UL * 60UL * 1000UL;
  static constexpr int MED_WINDOW = 7;
  static constexpr float MAX_RINT25_mOHM =
      200.0f; // sanity limit: reject Rint25 > 200mOhm
  static constexpr float MIN_RINT25_mOHM =
      3.0f; // sanity limit: reject Rint25 < 3mOhm (unrealistically low)
  static constexpr float MIN_RINT25_VS_BASELINE_RATIO =
      0.4f; // reject measurements < 40% of baseline (likely bad readings)
  static constexpr float MAX_RINT25_VS_BASELINE_RATIO =
      2.5f; // reject measurements > 2.5x baseline (likely measurement artifacts during cranking/high loads)

  // ---- State ----
  Sample _rb[RB_CAPACITY];
  int _head = 0;
  int _count = 0;
  float _baseline_mOhm = INITIAL_BASELINE_mOHM;
  float _lastRint_mOhm = NAN;
  float _lastRint25_mOhm = NAN;
  uint32_t _lastBaselineUpdateMs = 0;
  float _recentR25[MED_WINDOW] = {0};
  int _recentCount = 0;
  DebugPublisher *_dbg = nullptr;
  Preferences prefs;

  // ---- Helpers ----
  void push(const Sample &s) {
    _rb[_head] = s;
    _head = (_head + 1) % RB_CAPACITY;
    if (_count < RB_CAPACITY)
      _count++;
  }

  bool getBack(int backIdx, Sample &out) const {
    if (backIdx < 0 || backIdx >= _count)
      return false;
    int idx = _head - 1 - backIdx;
    if (idx < 0)
      idx += RB_CAPACITY;
    out = _rb[idx];
    return true;
  }

  static float compTo25C(float R_mOhm, float tempC) {
    float f = 1.0f + TEMP_ALPHA_PER_C * (tempC - REF_TEMP_C);
    if (f < 0.5f)
      f = 0.5f;
    if (f > 1.5f)
      f = 1.5f;
    return R_mOhm / f;
  }

  struct Stats {
    float v = 0;
    float i = 0;
    float t = 0;
    int n = 0;
  };

  Stats avgOverBackRange(int idxStart, int idxEnd) {
    Stats st;
    for (int b = idxStart; b >= idxEnd; --b) {
      Sample s;
      if (!getBack(b, s))
        continue;
      st.v += s.V;
      st.i += s.I;
      st.t += s.T;
      st.n++;
    }
    if (st.n > 0) {
      st.v /= st.n;
      st.i /= st.n;
      st.t /= st.n;
    }
    return st;
  }

  bool alternatorOn(const Stats &st) { return (st.v >= ALT_ON_VOLTAGE_V); }

  bool windowByTime(int anchorBackIdx, uint32_t ms, bool pre, int &outIdx) {
    Sample anchor;
    if (!getBack(anchorBackIdx, anchor))
      return false;
    if (pre) {
      int b = anchorBackIdx;
      Sample s;
      while (b < _count) {
        if (!getBack(b, s))
          break;
        uint32_t dt = anchor.t - s.t;
        if (dt >= ms) {
          outIdx = b;
          return true;
        }
        ++b;
      }
      outIdx = std::min(_count - 1, anchorBackIdx + 20);
      return true;
    } else {
      int b = anchorBackIdx;
      Sample s;
      while (b >= 0) {
        if (!getBack(b, s))
          break;
        uint32_t dt = s.t - anchor.t;
        if (dt >= ms) {
          outIdx = b;
          return true;
        }
        --b;
      }
      outIdx = std::max(0, anchorBackIdx - 20);
      return true;
    }
  }

  bool findStepWindows(int &preStart, int &preEnd, int &postStart,
                       int &postEnd) {
    if (_count < 10)
      return false;
    Sample newest;
    getBack(0, newest);
    uint32_t tNow = newest.t;
    for (int back = 3; back < _count; ++back) {
      Sample sPost;
      if (!getBack(back, sPost))
        break;
      if (tNow - sPost.t > SCAN_BACK_MS)
        break;
      int backEarlier = back + 10;
      Sample sPre;
      for (int b2 = back + 1; b2 < _count; ++b2) {
        if (!getBack(b2, sPre))
          break;
        uint32_t dt = sPost.t - sPre.t;
        if (dt >= STEP_SEPARATION_MS) {
          backEarlier = b2;
          break;
        }
      }
      if (!getBack(backEarlier, sPre))
        continue;
      float dI = sPost.I - sPre.I;
      if (fabsf(dI) < I_STEP_MIN_A)
        continue;
      preEnd = backEarlier;
      postStart = back;
      if (!windowByTime(preEnd, STEP_WINDOW_MS, true, preStart))
        continue;
      if (!windowByTime(postStart, STEP_WINDOW_MS, false, postEnd))
        continue;
      if (_dbg && _dbg->ok()) {
        char js[256];
        snprintf(
            js, sizeof(js),
            R"({"event":"step_detected","preEnd":%d,"postStart":%d,"dI":%.3f,"preIdx":%d,"postIdx":%d})",
            preEnd, postStart, dI, preStart, postEnd);
        _dbg->send(js, "step_detected");
      }
      return true;
    }
    return false;
  }

  void addRecentR25(float v) {
    if (_recentCount < MED_WINDOW) {
      _recentR25[_recentCount++] = v;
    } else {
      for (int i = 1; i < MED_WINDOW; i++)
        _recentR25[i - 1] = _recentR25[i];
      _recentR25[MED_WINDOW - 1] = v;
    }
  }

  float medianRecentR25() const {
    if (_recentCount <= 0)
      return _lastRint25_mOhm;
    float tmp[MED_WINDOW];
    int n = _recentCount;
    for (int i = 0; i < n; i++)
      tmp[i] = _recentR25[i];
    for (int i = 1; i < n; i++) {
      float key = tmp[i];
      int j = i - 1;
      while (j >= 0 && tmp[j] > key) {
        tmp[j + 1] = tmp[j];
        j--;
      }
      tmp[j + 1] = key;
    }
    return (n % 2 == 1) ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
  }

  void tryDetectAndLearn(uint32_t nowMs) {
    int preStart, preEnd, postStart, postEnd;
    if (!findStepWindows(preStart, preEnd, postStart, postEnd))
      return;
    Stats pre = avgOverBackRange(preStart, preEnd);
    Stats post = avgOverBackRange(postStart, postEnd);
    if (pre.n < 3 || post.n < 3) {
      debugReject("few_samples", pre.n, post.n);
      return;
    }
    if (alternatorOn(pre) || alternatorOn(post)) {
      debugReject("alt_on", pre.v, post.v);
      return;
    }
    float dI = post.i - pre.i;
    float dV = post.v - pre.v;
    if (fabsf(post.i) < I_ABS_MIN_A && fabsf(pre.i) < I_ABS_MIN_A) {
      debugReject("abs_I_low", post.i, pre.i);
      return;
    }
    if (fabsf(dI) > MAX_EVENT_dI_A) {
      debugReject("di_too_big", dI, MAX_EVENT_dI_A);
      return;
    }
    if (fabsf(dV) > MAX_EVENT_dV_V) {
      debugReject("dv_too_big", dV, MAX_EVENT_dV_V);
      return;
    }
    if (fabsf(dI) < 1e-6f) {
      debugReject("di_zero", dI, 0);
      return;
    }
    float R_mOhm = fabsf((dV / dI) * 1000.0f);
    if (!isfinite(R_mOhm) || R_mOhm <= 0.1f || R_mOhm > 1000.0f) {
      debugReject("r_out_of_range", R_mOhm, 0);
      return;
    }
    float Tmean = (pre.t + post.t) * 0.5f;
    float R25_mOhm = compTo25C(R_mOhm, Tmean);

    // Validate temperature-compensated Rint before storing
    if (!isfinite(R25_mOhm) || R25_mOhm <= 0.0f || R25_mOhm > MAX_RINT25_mOHM) {
      debugReject("r25_out_of_range", R25_mOhm, MAX_RINT25_mOHM);
      return;
    }

    // Reject unrealistically low measurements (likely bad readings)
    if (R25_mOhm < MIN_RINT25_mOHM) {
      debugReject("r25_too_low", R25_mOhm, MIN_RINT25_mOHM);
      return;
    }

    // Reject measurements significantly below baseline (physically implausible)
    // A healthy battery's Rint should never be much lower than its baseline
    float minAcceptable = _baseline_mOhm * MIN_RINT25_VS_BASELINE_RATIO;
    if (R25_mOhm < minAcceptable) {
      debugReject("r25_below_baseline", R25_mOhm, minAcceptable);
      return;
    }

    // Reject measurements significantly above baseline (measurement artifacts)
    // Spikes during cranking or high transients can produce false high readings
    float maxAcceptable = _baseline_mOhm * MAX_RINT25_VS_BASELINE_RATIO;
    if (R25_mOhm > maxAcceptable) {
      debugReject("r25_above_baseline", R25_mOhm, maxAcceptable);
      return;
    }

    _lastRint_mOhm = R_mOhm;
    _lastRint25_mOhm = R25_mOhm;
    // Save to NVM for persistence across deep sleep
    prefs.putFloat("lastRint_mR", R_mOhm);
    prefs.putFloat("lastR25_mR", R25_mOhm);
    if (_dbg && _dbg->ok()) {
      char js[320];
      snprintf(
          js, sizeof(js),
          R"({"event":"rint_computed","dV":%.4f,"dI":%.3f,"R_mOhm":%.2f,"R25_mOhm":%.2f,"Tmean_C":%.1f,"preV":%.3f,"preI":%.3f,"postV":%.3f,"postI":%.3f})",
          dV, dI, R_mOhm, R25_mOhm, Tmean, pre.v, pre.i, post.v, post.i);
      _dbg->send(js, "rint_computed");
    }
    addRecentR25(R25_mOhm);
    float candidate = medianRecentR25();
    maybeUpdateBaseline(candidate, nowMs);
  }

  void maybeUpdateBaseline(float candidate_mOhm, uint32_t nowMs) {
    if (nowMs - _lastBaselineUpdateMs < MIN_UPDATE_INTERVAL_MS)
      return;
    float delta = candidate_mOhm - _baseline_mOhm;
    if (fabsf(delta) < BASELINE_DEADBAND_mOHM)
      return;
    float alpha = (delta > 0) ? EWMA_UP_ALPHA : EWMA_DOWN_ALPHA;
    float proposed = _baseline_mOhm + alpha * delta;
    float hours = (nowMs - _lastBaselineUpdateMs) / 3600000.0f;
    if (hours <= 0.0f)
      hours = MIN_UPDATE_INTERVAL_MS / 3600000.0f;
    float riseCap = _baseline_mOhm * MAX_RISE_PER_HR * hours;
    float fallCap = _baseline_mOhm * MAX_FALL_PER_HR * hours;
    float capped = proposed;
    if (proposed > _baseline_mOhm)
      capped = fminf(proposed, _baseline_mOhm + riseCap);
    else
      capped = fmaxf(proposed, _baseline_mOhm - fallCap);
    if (_dbg && _dbg->ok()) {
      char js[320];
      snprintf(
          js, sizeof(js),
          R"({"event":"baseline_update","old":%.2f,"cand":%.2f,"delta":%.2f,"alpha":%.3f,"proposed":%.2f,"capped":%.2f,"riseCap":%.3f,"fallCap":%.3f})",
          _baseline_mOhm, candidate_mOhm, delta, alpha, proposed, capped,
          riseCap, fallCap);
      _dbg->send(js, "baseline_update");
    }
    _baseline_mOhm = capped;
    prefs.putFloat("rintBase_mR", _baseline_mOhm);
    _lastBaselineUpdateMs = nowMs;
  }

  void debugReject(const char *reason, float a, float b) {
    if (!(_dbg && _dbg->ok()))
      return;
    char js[192];
    snprintf(js, sizeof(js),
             R"({"event":"step_rejected","reason":"%s","a":%.4f,"b":%.4f})",
             reason, a, b);
    _dbg->send(js, "step_rejected");
  }
};

// Global instance declared in main.cpp
extern RintLearner learner;
