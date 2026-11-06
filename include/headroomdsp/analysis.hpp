#pragma once
#include <string>
#include <vector>

namespace headroom {

// Summary statistics returned by the public C++ API.
// Uses BS.1770 / EBU-style analysis; true-peak FIR requires formal conformance testing.
// - Loudness in LUFS / LU
// - True peak in dBTP
// - Duration in seconds
struct AnalysisResult {
  double integrated_lufs;    // Programme loudness, gated (I)
  double short_term_lufs;    // 95th percentile of 3 s windows, -inf if audio < 3s
  double momentary_lufs;     // Momentary loudness (0.4 s, M)
  double true_peak_dbtp;     // Max true peak level [dBTP]
  double loudness_range_lu;  // LRA, in Loudness Units
  double crest_factor_db;    // API field name: peak-to-loudness ratio (PLR), not RMS crest factor
  double phase_correlation;  // -1 .. 1
  double duration_sec;       // seconds
};

// Time-series loudness measurements and derived diagnostics.
struct AdvancedAnalysisResult {
  // Timeline sampled every hop_sec (typically 0.1s)
  double hop_sec = 0.1;

  std::vector<double> t_sec;
  std::vector<double> momentary_lufs;   // 400ms window (per hop)
  std::vector<double> short_term_lufs;  // 3s window (per hop)
  std::vector<double> true_peak_dbfs;   // API field name: per-hop SAMPLE peak, NOT true peak

  // Derived
  double limiter_stress_0_100 = 0.0;

  // Hotspots
  double loudest_sec = 0.0;
  double quietest_sec = 0.0;
  double max_jump_lu = 0.0;
  double max_jump_at_sec = 0.0;

  struct PlatformPreview {
    std::string name;
    double target_lufs;
    double gain_db;
    double normalized_integrated_lufs;
    double true_peak_after_dbtp;
  };
  std::vector<PlatformPreview> platform;
};

// Main entry point for the DSP core.
// `left` and `right` should be in the range [-1, 1],
// with 0 dBFS at |sample| == 1.0.
AnalysisResult analyze_stereo(
  const std::vector<float>& left,
  const std::vector<float>& right,
  int sample_rate
);

// Time-series analysis entry point.
AdvancedAnalysisResult analyze_advanced_stereo(
  const std::vector<float>& left,
  const std::vector<float>& right,
  int sample_rate
);

} // namespace headroom
