#include "headroomdsp/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numeric>

#include <vector>
#include <string>
#include <utility>

#include <limits>

namespace headroom {

// =========================
// Utility helpers
// =========================

namespace {

constexpr double EPS = 1e-12;
const double NEG_INF = -std::numeric_limits<double>::infinity();

// Clamp to [lo, hi]
inline double clamp(double x, double lo, double hi) {
    return std::min(hi, std::max(lo, x));
}

// Safe log10
inline double safe_log10(double x) {
    return std::log10(std::max(x, EPS));
}

// K-weighted loudness uses this constant from BS.1770:
// LKFS = -0.691 + 10*log10( sum( channel_powers ) )
constexpr double LOUDNESS_OFFSET = -0.691;  // dB

// Simple biquad filter, Direct Form I.
// This makes the K-weighting implementation readable.
struct Biquad {
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;

    double z1 = 0.0;  // delay 1
    double z2 = 0.0;  // delay 2

    inline float process(float x) {
        // Direct Form I implementation:
        // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float>(y);
    }
};

// K-weighting based on the analogue prototype and bilinear transform.
// The constants reproduce the BS.1770 48 kHz coefficients and give
// sample-rate-specific responses at other sample rates.
void make_k_weighting_filters(int sample_rate,
                              Biquad& shelf,
                              Biquad& highpass) {
    constexpr double pi = 3.1415926535897932384626433832795;
    const double fs = static_cast<double>(sample_rate);
    const double k1 = std::tan(pi * 1681.974450955533 / fs);
    const double q1 = 0.7071752369554196;
    const double vh = std::pow(10.0, 3.999843853973347 / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + k1 / q1 + k1 * k1;
    shelf.b0 = (vh + vb * k1 / q1 + k1*k1) / a0;
    shelf.b1 = 2.0 * (k1*k1 - vh) / a0;
    shelf.b2 = (vh - vb * k1 / q1 + k1*k1) / a0;
    shelf.a1 = 2.0 * (k1*k1 - 1.0) / a0;
    shelf.a2 = (1.0 - k1 / q1 + k1*k1) / a0;

    const double k2 = std::tan(pi * 38.13547087602444 / fs);
    const double q2 = 0.5003270373238773;
    const double d = 1.0 + k2 / q2 + k2*k2;
    // Retain BS.1770's unity numerator at 48 kHz (rather than
    // dividing all b coefficients by d).
    highpass.b0 = 1.0;
    highpass.b1 = -2.0;
    highpass.b2 = 1.0;
    highpass.a1 = 2.0 * (k2*k2 - 1.0) / d;
    highpass.a2 = (1.0 - k2 / q2 + k2*k2) / d;
}

// Apply K-weighting to one channel, in-place.
void apply_k_weighting(std::vector<float>& x, int sample_rate) {
    Biquad s1, s2;
    make_k_weighting_filters(sample_rate, s1, s2);

    for (auto& sample : x) {
        float y = s1.process(sample);
        y = s2.process(y);
        sample = y;
    }
}

// Compute simple phase correlation for stereo.
// This is *not* part of BS.1770, but it’s a useful UX signal.
double compute_phase_correlation(const std::vector<float>& L,
                                 const std::vector<float>& R) {
    if (L.size() != R.size() || L.empty())
        return 0.0;

    long double sum_LR = 0.0L;
    long double sum_L2 = 0.0L;
    long double sum_R2 = 0.0L;

    const size_t n = L.size();
    for (size_t i = 0; i < n; ++i) {
        const long double l = L[i];
        const long double r = R[i];
        sum_LR += l * r;
        sum_L2 += l * l;
        sum_R2 += r * r;
    }

    const long double denom = std::sqrt(sum_L2 * sum_R2);
    if (denom <= 0.0L)
        return 0.0;

    const double corr = static_cast<double>(sum_LR / denom);
    return clamp(corr, -1.0, 1.0);
}

// 4x polyphase windowed-sinc FIR reconstruction.
// Linear interpolation cannot detect inter-sample overshoot. This offline
// FIR estimator uses 32 neighbouring input samples for each of 3 extra
// fractional phases and also checks original samples. The estimator is
// NOT claimed as certified BS.1770 true-peak conformance; compare it with
// the official true-peak test vectors before certifying a meter.
double compute_true_peak_fir4x(const std::vector<float>& L,
                              const std::vector<float>& R) {
    if (L.size() != R.size() || L.empty()) return NEG_INF;
    constexpr int taps = 32;
    constexpr int half = taps / 2;
    constexpr double pi = 3.1415926535897932384626433832795;
    double coefficients[3][taps] = {};
    for (int phase = 1; phase < 4; ++phase) {
        const double fraction = static_cast<double>(phase) / 4.0;
        double sum = 0.0;
        for (int tap = 0; tap < taps; ++tap) {
            // Relative input indices are -15 .. +16.
            const double dist = fraction - static_cast<double>(tap - (half - 1));
            const double sinc = std::abs(dist) < 1e-12 ? 1.0 :
                                std::sin(pi * dist) / (pi * dist);
            // 3-term Blackman window (zero beyond the reconstruction radius).
            const double w = 0.42 + 0.5 * std::cos(pi * dist / half) +
                             0.08 * std::cos(2.0 * pi * dist / half);
            const double coeff = std::abs(dist) <= half ? sinc * w : 0.0;
            coefficients[phase-1][tap] = coeff;
            sum += coeff;
        }
        for (double& coeff : coefficients[phase-1]) coeff /= sum;
    }

    double peak = 0.0;
    const std::size_t n = L.size();
    for (std::size_t i = 0; i < n; ++i) {
        peak = std::max(peak, std::abs(static_cast<double>(L[i])));
        peak = std::max(peak, std::abs(static_cast<double>(R[i])));
        // No interpolated phase is needed after the final sample.
        if (i + 1 == n) break;
        for (int phase = 0; phase < 3; ++phase) {
            double vL = 0.0, vR = 0.0;
            for (int tap = 0; tap < taps; ++tap) {
                const std::ptrdiff_t idx = static_cast<std::ptrdiff_t>(i) +
                                           tap - (half - 1);
                if (idx < 0 || static_cast<std::size_t>(idx) >= n) continue;
                const double coeff = coefficients[phase][tap];
                vL += coeff * L[static_cast<std::size_t>(idx)];
                vR += coeff * R[static_cast<std::size_t>(idx)];
            }
            peak = std::max(peak, std::max(std::abs(vL), std::abs(vR)));
        }
    }
    return peak > 0.0 ? 20.0 * std::log10(peak) : NEG_INF;
}

// Helper to compute loudness (LUFS) from per-channel powers.
inline double loudness_from_channel_powers(const std::vector<double>& channel_powers) {
    double sum_power = 0.0;
    for (double p : channel_powers) {
        sum_power += p;  // weight = 1.0 for both channels here
    }
    if (sum_power <= 0.0)
        return NEG_INF;

    return LOUDNESS_OFFSET + 10.0 * safe_log10(sum_power);
}

// Compute mean-square power of a segment [start, start+len) for one channel.
double block_power(const std::vector<float>& x, size_t start, size_t len) {
    long double acc = 0.0L;
    const size_t end = std::min(x.size(), start + len);
    const size_t n = (end > start) ? (end - start) : 0;
    if (n == 0)
        return 0.0;

    for (size_t i = start; i < end; ++i) {
        const long double v = x[i];
        acc += v * v;
    }
    return static_cast<double>(acc / static_cast<long double>(n));
}

// Percentile helper (0.0–1.0).
double percentile(std::vector<double> values, double p) {
    if (values.empty())
        return 0.0;
    if (p <= 0.0)
        return *std::min_element(values.begin(), values.end());
    if (p >= 1.0)
        return *std::max_element(values.begin(), values.end());

    std::sort(values.begin(), values.end());
    const double idx = p * (values.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    const double frac = idx - static_cast<double>(lo);

    if (hi >= values.size())
        return values.back();
    // Avoid -inf * 0 -> NaN for silent windows.
    if (lo == hi || values[lo] == values[hi]) return values[lo];
    if (!std::isfinite(values[lo])) return values[hi];
    if (!std::isfinite(values[hi])) return values[lo];
    return values[lo] * (1.0 - frac) + values[hi] * frac;
}

} // namespace

// =========================
// Public API
// =========================

AnalysisResult analyze_stereo(
    const std::vector<float>& left_in,
    const std::vector<float>& right_in,
    int sample_rate
) {
    if (left_in.size() != right_in.size()) {
        throw std::invalid_argument("Left/right buffers must be same length");
    }
    if (sample_rate < 8000 || sample_rate > 384000) {
        throw std::invalid_argument("sample_rate must be in [8000, 384000]");
    }
    const size_t n = left_in.size();
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(left_in[i]) || !std::isfinite(right_in[i])) {
            throw std::invalid_argument("Audio samples must be finite");
        }
    }
    if (n == 0) {
        throw std::invalid_argument("Empty input buffers");
    }

    // Copy inputs so we can K-weight in-place.
    std::vector<float> L = left_in;
    std::vector<float> R = right_in;

    // Duration in seconds.
    const double duration_sec = static_cast<double>(n) /
                                static_cast<double>(sample_rate);

    // 1) Phase correlation (on raw signal, more intuitive for users)
    const double phase_corr = compute_phase_correlation(L, R);

    // 2) K-weighting (mutates L and R).
    apply_k_weighting(L, sample_rate);
    apply_k_weighting(R, sample_rate);

    // 3) True peak (pre-K-weighted signal).
    const double true_peak_dbtp = compute_true_peak_fir4x(left_in, right_in);

    // 4) Block configuration
    const size_t win400  = static_cast<size_t>(std::round(0.400 * sample_rate));
    const size_t hop100  = static_cast<size_t>(std::round(0.100 * sample_rate));
    const size_t win3sec = static_cast<size_t>(std::round(3.000 * sample_rate));

    if (win400 == 0 || hop100 == 0 || win3sec == 0) {
        throw std::runtime_error("Window configuration invalid for this sample rate");
    }

    // 5) 400 ms blocks → loudness values (ungated).
    std::vector<double> block_loudness_400;
    std::vector<double> block_powers_400;

    for (size_t start = 0; start + win400 <= n; start += hop100) {
        const double pL = block_power(L, start, win400);
        const double pR = block_power(R, start, win400);

        const double L_block = loudness_from_channel_powers({pL, pR});

        block_loudness_400.push_back(L_block);
        block_powers_400.push_back(pL + pR);
    }

    if (block_loudness_400.empty()) {
        throw std::runtime_error("Audio too short for BS.1770 loudness (needs >= 400 ms)");
    }

    // 6) Integrated loudness with gating
    const double ABS_GATE_LUFS = -70.0;
    const double REL_GATE_LU   = -10.0;

    std::vector<double> abs_gated_powers;
    abs_gated_powers.reserve(block_powers_400.size());

    for (size_t i = 0; i < block_loudness_400.size(); ++i) {
        if (block_loudness_400[i] > ABS_GATE_LUFS) {
            abs_gated_powers.push_back(block_powers_400[i]);
        }
    }

    double integrated_lufs = NEG_INF;
    if (!abs_gated_powers.empty()) {
        double sum_power = std::accumulate(abs_gated_powers.begin(),
                                           abs_gated_powers.end(), 0.0);
        double mean_power = sum_power / static_cast<double>(abs_gated_powers.size());

        integrated_lufs = LOUDNESS_OFFSET + 10.0 * safe_log10(mean_power);

        const double rel_gate_threshold = integrated_lufs + REL_GATE_LU;

        std::vector<double> rel_gated_powers;
        rel_gated_powers.reserve(abs_gated_powers.size());

        for (size_t i = 0; i < block_loudness_400.size(); ++i) {
            if (block_loudness_400[i] > ABS_GATE_LUFS &&
                block_loudness_400[i] > rel_gate_threshold) {
                rel_gated_powers.push_back(block_powers_400[i]);
            }
        }

        if (!rel_gated_powers.empty()) {
            sum_power = std::accumulate(rel_gated_powers.begin(),
                                        rel_gated_powers.end(), 0.0);
            mean_power = sum_power / static_cast<double>(rel_gated_powers.size());

            integrated_lufs = LOUDNESS_OFFSET + 10.0 * safe_log10(mean_power);
        }
    }

    // 7) Momentary summary: 95th percentile of 400ms blocks
    double momentary_lufs = percentile(block_loudness_400, 0.95);

    // 8) Short-term loudness (3s) and LRA
    std::vector<double> st_loudness;
    std::vector<double> st_loudness_gated;

    for (size_t start = 0; start + win3sec <= n; start += hop100) {
        const double pL = block_power(L, start, win3sec);
        const double pR = block_power(R, start, win3sec);
        const double L_block = loudness_from_channel_powers({pL, pR});
        st_loudness.push_back(L_block);
    }

    double short_term_lufs = NEG_INF;
    double lra_lu = 0.0;

    if (!st_loudness.empty()) {
        short_term_lufs = percentile(st_loudness, 0.95);

        const double LRA_ABS_GATE_LUFS = -70.0;
        const double LRA_REL_GATE_LU   = -20.0;

        std::vector<double> abs_gated_st;
        for (double L_block : st_loudness) {
            if (L_block > LRA_ABS_GATE_LUFS) {
                abs_gated_st.push_back(L_block);
            }
        }

        if (!abs_gated_st.empty()) {
            double sum_power = 0.0;
            for (double L_block : abs_gated_st) {
                const double power = std::pow(10.0, (L_block - LOUDNESS_OFFSET) / 10.0);
                sum_power += power;
            }
            const double mean_power = sum_power / static_cast<double>(abs_gated_st.size());
            const double st_integrated = LOUDNESS_OFFSET + 10.0 * safe_log10(mean_power);

            const double rel_gate_thres = st_integrated + LRA_REL_GATE_LU;

            for (double L_block : abs_gated_st) {
                if (L_block > rel_gate_thres) {
                    st_loudness_gated.push_back(L_block);
                }
            }

            if (!st_loudness_gated.empty()) {
                const double p10 = percentile(st_loudness_gated, 0.10);
                const double p95 = percentile(st_loudness_gated, 0.95);
                lra_lu = p95 - p10;
            }
        }
    }

    // 9) Peak-to-loudness ratio, not conventional peak-to-RMS crest factor
    double crest_factor_db = (std::isfinite(true_peak_dbtp) &&
                              std::isfinite(integrated_lufs)) ?
                              true_peak_dbtp - integrated_lufs : 0.0;

    AnalysisResult res;
    res.integrated_lufs    = integrated_lufs;
    res.short_term_lufs    = short_term_lufs;
    res.momentary_lufs     = momentary_lufs;
    res.true_peak_dbtp     = true_peak_dbtp;
    res.loudness_range_lu  = lra_lu;
    res.crest_factor_db    = crest_factor_db;
    res.phase_correlation  = phase_corr;
    res.duration_sec       = duration_sec;
    return res;
}

// =========================
// Advanced analysis (ONE definition only)
// =========================

AdvancedAnalysisResult analyze_advanced_stereo(
    const std::vector<float>& left_in,
    const std::vector<float>& right_in,
    int sample_rate
) {
    if (left_in.size() != right_in.size()) {
        throw std::invalid_argument("Left/right buffers must be same length");
    }
    if (sample_rate < 8000 || sample_rate > 384000) {
        throw std::invalid_argument("sample_rate must be in [8000, 384000]");
    }
    const size_t n = left_in.size();
    for (size_t i = 0; i < n; ++i) {
        if (!std::isfinite(left_in[i]) || !std::isfinite(right_in[i])) {
            throw std::invalid_argument("Audio samples must be finite");
        }
    }
    if (n == 0) {
        throw std::invalid_argument("Empty input buffers");
    }

    // Reuse “basic” analysis for consistent headline metrics.
    const AnalysisResult base = analyze_stereo(left_in, right_in, sample_rate);

    // Copy for K-weighting
    std::vector<float> L = left_in;
    std::vector<float> R = right_in;

    apply_k_weighting(L, sample_rate);
    apply_k_weighting(R, sample_rate);

    const size_t win400  = static_cast<size_t>(std::round(0.400 * sample_rate));
    const size_t hop100  = static_cast<size_t>(std::round(0.100 * sample_rate));
    const size_t win3sec = static_cast<size_t>(std::round(3.000 * sample_rate));

    if (win400 == 0 || hop100 == 0 || win3sec == 0) {
        throw std::runtime_error("Window configuration invalid for this sample rate");
    }

    AdvancedAnalysisResult out;
    out.hop_sec = static_cast<double>(hop100) / static_cast<double>(sample_rate);

    // Only generate time points where full 3 s and 0.4 s windows fit.
    const bool has_short_term = n >= win3sec;
    const size_t max_start = has_short_term ? (n - win3sec) : 0;

    out.t_sec.reserve(max_start / hop100 + 1);
    out.momentary_lufs.reserve(max_start / hop100 + 1);
    out.short_term_lufs.reserve(max_start / hop100 + 1);
    out.true_peak_dbfs.reserve(max_start / hop100 + 1);

    auto dbfs_from_peak = [&](double peak) -> double {
        if (peak <= 0.0) return NEG_INF;
        return 20.0 * safe_log10(peak);
    };

    // helper: cheap per-hop peak (raw, no oversampling)
    auto hop_peak = [&](size_t start, size_t len) -> double {
        const size_t end = std::min(n, start + len);
        double pk = 0.0;
        for (size_t i = start; i < end; ++i) {
            pk = std::max(pk, std::abs(static_cast<double>(left_in[i])));
            pk = std::max(pk, std::abs(static_cast<double>(right_in[i])));
        }
        return pk;
    };

    // Build timeline
    for (size_t start = 0; has_short_term && start <= max_start; start += hop100) {
        const double t = static_cast<double>(start) / static_cast<double>(sample_rate);

        // momentary (400ms) using K-weighted signal
        const double pL_m = block_power(L, start, win400);
        const double pR_m = block_power(R, start, win400);
        const double m = loudness_from_channel_powers({pL_m, pR_m});

        // short-term (3s) using K-weighted signal
        const double pL_s = block_power(L, start, win3sec);
        const double pR_s = block_power(R, start, win3sec);
        const double s = loudness_from_channel_powers({pL_s, pR_s});

        // peak per hop (raw)
        const double pk = hop_peak(start, hop100);
        const double pk_dbfs = dbfs_from_peak(pk);

        out.t_sec.push_back(t);
        out.momentary_lufs.push_back(m);
        out.short_term_lufs.push_back(s);
        out.true_peak_dbfs.push_back(pk_dbfs);
    }

    // Hotspots: loudest/quietest by short-term
    if (!out.short_term_lufs.empty()) {
        size_t loud_i = 0, quiet_i = 0;
        for (size_t i = 1; i < out.short_term_lufs.size(); ++i) {
            if (out.short_term_lufs[i] > out.short_term_lufs[loud_i]) loud_i = i;
            if (out.short_term_lufs[i] < out.short_term_lufs[quiet_i]) quiet_i = i;
        }
        out.loudest_sec = out.t_sec[loud_i];
        out.quietest_sec = out.t_sec[quiet_i];

        double max_jump = 0.0;
        double max_at = 0.0;
        for (size_t i = 1; i < out.short_term_lufs.size(); ++i) {
            const double j = std::abs(out.short_term_lufs[i] - out.short_term_lufs[i - 1]);
            if (j > max_jump) {
                max_jump = j;
                max_at = out.t_sec[i];
            }
        }
        out.max_jump_lu = max_jump;
        out.max_jump_at_sec = max_at;
    } else {
        out.loudest_sec = 0.0;
        out.quietest_sec = 0.0;
        out.max_jump_lu = 0.0;
        out.max_jump_at_sec = 0.0;
    }

    // Limiter Stress Index (0–100), UX-driven heuristic
    const double tp = base.true_peak_dbtp;          // dBTP (often around -1 to 0)
    const double crest = base.crest_factor_db;      // peak - integrated (e.g. 8–14)
    const double lra = base.loudness_range_lu;      // LU (e.g. 4–12)

    const double tp_n = clamp((tp + 3.0) / 3.0, 0.0, 1.0);                 // -3 -> 0, 0 -> 1
    const double crest_n = 1.0 - clamp((crest - 6.0) / 10.0, 0.0, 1.0);    // 6..16
    const double lra_n = 1.0 - clamp((lra - 3.0) / 10.0, 0.0, 1.0);        // 3..13

    const double stress = (0.50 * tp_n) + (0.30 * crest_n) + (0.20 * lra_n);
    out.limiter_stress_0_100 = clamp(stress * 100.0, 0.0, 100.0);

    // Platform preview (Universal Safety Curve)
    const std::vector<std::pair<std::string,double>> targets = {
        {"Streaming -14", -14.0},
        {"Streaming -16", -16.0},
    };

    // Use the FIR-estimated true peak from the base analysis
    const double global_tp = base.true_peak_dbtp; 

    out.platform.clear();
    out.platform.reserve(targets.size());

    for (const auto& it : targets) {
        const double target_lufs = it.second;

        // 1. How much gain is needed to hit the LUFS target?
        const double gain_db = target_lufs - base.integrated_lufs;
        
        // 2. UNIVERSAL CURVE: Cap the gain so the peak never goes above -1.0.
        // If the track is quiet, it might not reach -14 LUFS, and that's OK.
        // If the track is loud, it will be turned down (gain_db will be negative).
        const double gain_limit = -1.0 - global_tp;
        const double effective_gain =
            (std::isfinite(gain_db) && std::isfinite(gain_limit)) ?
                std::min(gain_db, gain_limit) : 0.0;

        AdvancedAnalysisResult::PlatformPreview pp;
        pp.name = it.first;
        pp.target_lufs = target_lufs;
        pp.gain_db = effective_gain; 
        pp.normalized_integrated_lufs = base.integrated_lufs + effective_gain;
        pp.true_peak_after_dbtp = global_tp + effective_gain;

        out.platform.push_back(pp);
    }

    return out;
}

} // namespace headroom
