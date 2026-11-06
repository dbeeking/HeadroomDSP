#include "headroomdsp/analysis.hpp"
#include "headroomdsp/c_api.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace headroom;

namespace {
constexpr double pi = 3.14159265358979323846;
struct Audio { std::vector<float> l, r; };
Audio sine(int fs, double freq, double seconds, double amp, double phase = 0.0) {
    const auto n = static_cast<std::size_t>(fs * seconds);
    Audio a{std::vector<float>(n), std::vector<float>(n)};
    for (std::size_t i = 0; i < n; ++i) {
        const float value = static_cast<float>(amp * std::sin(2*pi*freq*i/fs + phase));
        a.l[i] = value; a.r[i] = value;
    }
    return a;
}
void near(double a, double b, double tol) { assert(std::abs(a - b) <= tol); }
void check(bool b, const char* msg) { if (!b) throw std::runtime_error(msg); }
}

int main() {
    const auto base = sine(48000, 1000, 4.0, 0.1);
    const auto r = analyze_stereo(base.l, base.r, 48000);
    check(std::isfinite(r.integrated_lufs), "finite integrated loudness");
    check(r.integrated_lufs > -23 && r.integrated_lufs < -18, "1k Hz stereo loudness sanity");
    near(r.duration_sec, 4.0, 0.00001);
    near(r.phase_correlation, 1.0, 1e-7);
    check(r.true_peak_dbtp > -20.1 && r.true_peak_dbtp < -19.5, "0.1 amplitude FIR peak");
    check(std::abs(r.loudness_range_lu) < 0.5, "constant loudness has small LRA");

    // Different sample rates should produce similar weighted loudness.
    for (int rate : {44100, 96000}) {
        const auto a = sine(rate, 1000, 4.0, 0.1);
        const auto other = analyze_stereo(a.l, a.r, rate);
        check(std::abs(other.integrated_lufs - r.integrated_lufs) < 0.5,
              "sample-rate-independent K weighting");
    }

    auto anticorr = base;
    for (float& v : anticorr.r) v = -v;
    near(analyze_stereo(anticorr.l, anticorr.r, 48000).phase_correlation, -1.0, 1e-7);

    const std::vector<float> zeros(48000 * 3, 0.0f);
    const auto silence = analyze_stereo(zeros, zeros, 48000);
    check(std::isinf(silence.integrated_lufs) && silence.integrated_lufs < 0,
          "silence must be -inf LUFS");
    check(std::isinf(silence.true_peak_dbtp) && silence.true_peak_dbtp < 0,
          "silence must be -inf dBTP");
    check(!std::isnan(silence.momentary_lufs), "silent momentary should not be NaN");
    check(!std::isnan(silence.short_term_lufs), "silent short term should not be NaN");

    // Same-sign adjacent samples can reconstruct an inter-sample overshoot.
    Audio isp{std::vector<float>(48000, 0.0f), std::vector<float>(48000, 0.0f)};
    isp.l[24000] = isp.l[24001] = 0.9f;
    isp.r[24000] = isp.r[24001] = 0.9f;
    const auto isp_result = analyze_stereo(isp.l, isp.r, 48000);
    check(isp_result.true_peak_dbtp > 20*std::log10(0.9)+0.05,
          "FIR true peak should detect intersample overshoot missed by linear interpolation");

    // Zero, invalid-rate, invalid samples and mismatched inputs are rejected.
    bool threw = false;
    try { analyze_stereo({}, {}, 48000); } catch (const std::invalid_argument&) { threw=true; }
    check(threw, "reject empty input");
    threw = false;
    try { analyze_stereo(base.l, base.r, 0); } catch (const std::invalid_argument&) { threw=true; }
    check(threw, "reject invalid rate");
    threw = false;
    auto bad = base;
    bad.l[0] = std::numeric_limits<float>::quiet_NaN();
    try { analyze_stereo(bad.l, bad.r, 48000); } catch (const std::invalid_argument&) { threw=true; }
    check(threw, "reject NaN");

    // Advanced derived analytics plus stable C API.
    const auto adv = analyze_advanced_stereo(base.l, base.r, 48000);
    check(!adv.t_sec.empty(), "advanced timeline exists");
    check(adv.t_sec.size() == adv.short_term_lufs.size(), "aligned timeline");
    check(adv.platform.size() == 2, "platform previews preserved");
    check(adv.limiter_stress_0_100 >= 0 && adv.limiter_stress_0_100 <= 100,
          "stress score bounds");
    const auto short_input = sine(48000, 500, 1, 0.1);
    check(analyze_advanced_stereo(short_input.l, short_input.r, 48000).t_sec.empty(),
          "no incomplete 3s windows");

    HRResult cr{};
    check(hr_analyze(base.l.data(), base.r.data(), static_cast<int>(base.l.size()), 48000, &cr)==0,
          "C API analysis");
    near(cr.integrated_lufs, r.integrated_lufs, 1e-8);
    check(hr_analyze(nullptr, nullptr, 100, 48000, &cr) != 0, "C API rejects null");
    const auto handle = hr_analyze_advanced(base.l.data(), base.r.data(), static_cast<int>(base.l.size()), 48000);
    check(handle != nullptr, "C API advanced handle");
    check(hr_adv_platform_count(handle) == 2, "C API platform preview");
    check(std::isnan(hr_adv_hop_sec(nullptr)), "null C API handle is safe");
    check(hr_adv_timeline_len(nullptr) == -1, "null C API length is safe");
    check(std::isnan(hr_adv_platform_gain_db(handle, -1)), "negative preview index is safe");
    char name[4] = {};
    hr_adv_platform_name(handle, 0, name, 4);
    check(name[3] == '\0', "platform name truncates safely");
    hr_adv_free(handle);
    std::cout << "HeadroomDSP tests passed\n";
}
