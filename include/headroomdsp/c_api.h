#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ── Basic analysis result (fixed-size struct) ── */
typedef struct {
    double integrated_lufs;
    double short_term_lufs;
    double momentary_lufs;
    double true_peak_dbtp;
    double loudness_range_lu;
    double crest_factor_db;
    double phase_correlation;
    double duration_sec;
} HRResult;

/* Returns 0 on success, -1 on error */
int hr_analyze(const float* left, const float* right, int n, int sample_rate, HRResult* out);

/* ── Advanced analysis (opaque handle + getters) ── */
typedef void* HRAdvanced;

HRAdvanced hr_analyze_advanced(const float* left, const float* right, int n, int sample_rate);

/* Scalars */
double hr_adv_hop_sec(HRAdvanced h);
double hr_adv_limiter_stress(HRAdvanced h);
double hr_adv_loudest_sec(HRAdvanced h);
double hr_adv_quietest_sec(HRAdvanced h);
double hr_adv_max_jump_lu(HRAdvanced h);
double hr_adv_max_jump_at_sec(HRAdvanced h);

/* Timeline arrays — caller allocates, length from hr_adv_timeline_len */
int    hr_adv_timeline_len(HRAdvanced h);
void   hr_adv_timeline_t(HRAdvanced h, double* out);
void   hr_adv_timeline_momentary(HRAdvanced h, double* out);
void   hr_adv_timeline_short_term(HRAdvanced h, double* out);
void   hr_adv_timeline_peak(HRAdvanced h, double* out);

/* Platform preview — always 2 entries (Streaming -14, Streaming -16) */
int    hr_adv_platform_count(HRAdvanced h);
double hr_adv_platform_target_lufs(HRAdvanced h, int i);
double hr_adv_platform_gain_db(HRAdvanced h, int i);
double hr_adv_platform_norm_lufs(HRAdvanced h, int i);
double hr_adv_platform_true_peak(HRAdvanced h, int i);
void   hr_adv_platform_name(HRAdvanced h, int i, char* out, int max_len);

void   hr_adv_free(HRAdvanced h);

#ifdef __cplusplus
}
#endif
