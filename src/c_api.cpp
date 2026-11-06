#include "headroomdsp/c_api.h"
#include "headroomdsp/analysis.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace {
using headroom::AdvancedAnalysisResult;
const AdvancedAnalysisResult* checked(HRAdvanced h) {
    return static_cast<const AdvancedAnalysisResult*>(h);
}
constexpr double invalid_value() { return std::numeric_limits<double>::quiet_NaN(); }
const AdvancedAnalysisResult::PlatformPreview* preview(HRAdvanced h, int i) {
    const auto* a = checked(h);
    if (!a || i < 0 || static_cast<std::size_t>(i) >= a->platform.size()) return nullptr;
    return &a->platform[static_cast<std::size_t>(i)];
}
void copy_array(HRAdvanced h, double* out, const std::vector<double> AdvancedAnalysisResult::*field) {
    if (!h || !out) return;
    const auto& v = checked(h)->*field;
    std::copy(v.begin(), v.end(), out);
}
}

extern "C" {
int hr_analyze(const float* left, const float* right, int n, int sample_rate, HRResult* out) {
    if (!left || !right || !out || n <= 0) return -1;
    try {
        std::vector<float> L(left, left + n), R(right, right + n);
        const auto r = headroom::analyze_stereo(L, R, sample_rate);
        out->integrated_lufs = r.integrated_lufs;
        out->short_term_lufs = r.short_term_lufs;
        out->momentary_lufs = r.momentary_lufs;
        out->true_peak_dbtp = r.true_peak_dbtp;
        out->loudness_range_lu = r.loudness_range_lu;
        out->crest_factor_db = r.crest_factor_db;
        out->phase_correlation = r.phase_correlation;
        out->duration_sec = r.duration_sec;
        return 0;
    } catch (...) {
        return -1;
    }
}

HRAdvanced hr_analyze_advanced(const float* left, const float* right, int n, int sample_rate) {
    if (!left || !right || n <= 0) return nullptr;
    try {
        std::vector<float> L(left, left + n), R(right, right + n);
        return new AdvancedAnalysisResult(headroom::analyze_advanced_stereo(L, R, sample_rate));
    } catch (...) {
        return nullptr;
    }
}

double hr_adv_hop_sec(HRAdvanced h) { return h ? checked(h)->hop_sec : invalid_value(); }
double hr_adv_limiter_stress(HRAdvanced h) { return h ? checked(h)->limiter_stress_0_100 : invalid_value(); }
double hr_adv_loudest_sec(HRAdvanced h) { return h ? checked(h)->loudest_sec : invalid_value(); }
double hr_adv_quietest_sec(HRAdvanced h) { return h ? checked(h)->quietest_sec : invalid_value(); }
double hr_adv_max_jump_lu(HRAdvanced h) { return h ? checked(h)->max_jump_lu : invalid_value(); }
double hr_adv_max_jump_at_sec(HRAdvanced h) { return h ? checked(h)->max_jump_at_sec : invalid_value(); }
int hr_adv_timeline_len(HRAdvanced h) {
    return h ? static_cast<int>(checked(h)->t_sec.size()) : -1;
}
void hr_adv_timeline_t(HRAdvanced h, double* out) { copy_array(h, out, &AdvancedAnalysisResult::t_sec); }
void hr_adv_timeline_momentary(HRAdvanced h, double* out) {
    copy_array(h, out, &AdvancedAnalysisResult::momentary_lufs);
}
void hr_adv_timeline_short_term(HRAdvanced h, double* out) {
    copy_array(h, out, &AdvancedAnalysisResult::short_term_lufs);
}
void hr_adv_timeline_peak(HRAdvanced h, double* out) {
    // Legacy name: actual sample peak, not 4x FIR true peak.
    copy_array(h, out, &AdvancedAnalysisResult::true_peak_dbfs);
}
int hr_adv_platform_count(HRAdvanced h) {
    return h ? static_cast<int>(checked(h)->platform.size()) : -1;
}
double hr_adv_platform_target_lufs(HRAdvanced h, int i) {
    const auto* p = preview(h, i);
    return p ? p->target_lufs : invalid_value();
}
double hr_adv_platform_gain_db(HRAdvanced h, int i) {
    const auto* p = preview(h, i);
    return p ? p->gain_db : invalid_value();
}
double hr_adv_platform_norm_lufs(HRAdvanced h, int i) {
    const auto* p = preview(h, i);
    return p ? p->normalized_integrated_lufs : invalid_value();
}
double hr_adv_platform_true_peak(HRAdvanced h, int i) {
    const auto* p = preview(h, i);
    return p ? p->true_peak_after_dbtp : invalid_value();
}
void hr_adv_platform_name(HRAdvanced h, int i, char* out, int max_len) {
    if (!out || max_len <= 0) return;
    out[0] = '\0';
    const auto* p = preview(h, i);
    if (!p) return;
    const std::size_t len = std::min(p->name.size(), static_cast<std::size_t>(max_len - 1));
    std::memcpy(out, p->name.c_str(), len);
    out[len] = '\0';
}
void hr_adv_free(HRAdvanced h) { delete static_cast<AdvancedAnalysisResult*>(h); }
}
