// cpp/headroom_dsp/bindings.cpp
//
// Pybind11 glue that exposes the C++ DSP core to Python.

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include <cstring>      // std::memcpy
#include <stdexcept>    // std::invalid_argument
#include <vector>

#include "headroomdsp/analysis.hpp"  // our BS.1770 / EBU implementation

namespace py = pybind11;
using namespace headroom;

// -----------------------------------------------------
// Helpers: accept NumPy arrays and forward to C++
// -----------------------------------------------------

static void numpy_to_vectors(
    const py::array_t<float, py::array::c_style | py::array::forcecast>& left,
    const py::array_t<float, py::array::c_style | py::array::forcecast>& right,
    std::vector<float>& L,
    std::vector<float>& R
) {
    if (left.ndim() != 1 || right.ndim() != 1) {
        throw std::invalid_argument("left/right must be 1D arrays");
    }
    if (left.shape(0) != right.shape(0)) {
        throw std::invalid_argument("left/right must be same length");
    }

    const std::size_t n = static_cast<std::size_t>(left.shape(0));
    L.resize(n);
    R.resize(n);

    std::memcpy(L.data(), left.data(),  n * sizeof(float));
    std::memcpy(R.data(), right.data(), n * sizeof(float));
}

static AnalysisResult analyze_numpy(
    py::array_t<float, py::array::c_style | py::array::forcecast> left,
    py::array_t<float, py::array::c_style | py::array::forcecast> right,
    int sample_rate
) {
    std::vector<float> L, R;
    numpy_to_vectors(left, right, L, R);
    return analyze_stereo(L, R, sample_rate);
}

static AdvancedAnalysisResult analyze_advanced_numpy(
    py::array_t<float, py::array::c_style | py::array::forcecast> left,
    py::array_t<float, py::array::c_style | py::array::forcecast> right,
    int sample_rate
) {
    std::vector<float> L, R;
    numpy_to_vectors(left, right, L, R);
    return analyze_advanced_stereo(L, R, sample_rate);
}

// -----------------------------------------------------
// Module
// -----------------------------------------------------

PYBIND11_MODULE(headroom_dsp, m) {
    m.doc() = "Headroom BS.1770 / EBU-R128 loudness analysis core";

    // Base result
    py::class_<AnalysisResult>(m, "AnalysisResult")
        .def_readonly("integrated_lufs",   &AnalysisResult::integrated_lufs)
        .def_readonly("short_term_lufs",   &AnalysisResult::short_term_lufs)
        .def_readonly("momentary_lufs",    &AnalysisResult::momentary_lufs)
        .def_readonly("true_peak_dbtp",    &AnalysisResult::true_peak_dbtp)
        .def_readonly("loudness_range_lu", &AnalysisResult::loudness_range_lu)
        .def_readonly("crest_factor_db",   &AnalysisResult::crest_factor_db)
        .def_readonly("phase_correlation", &AnalysisResult::phase_correlation)
        .def_readonly("duration_sec",      &AnalysisResult::duration_sec);

    // Advanced result nested struct
    py::class_<AdvancedAnalysisResult::PlatformPreview>(m, "PlatformPreview")
        .def_readonly("name", &AdvancedAnalysisResult::PlatformPreview::name)
        .def_readonly("target_lufs", &AdvancedAnalysisResult::PlatformPreview::target_lufs)
        .def_readonly("gain_db", &AdvancedAnalysisResult::PlatformPreview::gain_db)
        .def_readonly("normalized_integrated_lufs", &AdvancedAnalysisResult::PlatformPreview::normalized_integrated_lufs)
        .def_readonly("true_peak_after_dbtp", &AdvancedAnalysisResult::PlatformPreview::true_peak_after_dbtp);

    py::class_<AdvancedAnalysisResult>(m, "AdvancedAnalysisResult")
        .def_readonly("hop_sec", &AdvancedAnalysisResult::hop_sec)
        .def_readonly("t_sec", &AdvancedAnalysisResult::t_sec)
        .def_readonly("momentary_lufs", &AdvancedAnalysisResult::momentary_lufs)
        .def_readonly("short_term_lufs", &AdvancedAnalysisResult::short_term_lufs)
        .def_readonly("true_peak_dbfs", &AdvancedAnalysisResult::true_peak_dbfs)
        .def_readonly("limiter_stress_0_100", &AdvancedAnalysisResult::limiter_stress_0_100)
        .def_readonly("loudest_sec", &AdvancedAnalysisResult::loudest_sec)
        .def_readonly("quietest_sec", &AdvancedAnalysisResult::quietest_sec)
        .def_readonly("max_jump_lu", &AdvancedAnalysisResult::max_jump_lu)
        .def_readonly("max_jump_at_sec", &AdvancedAnalysisResult::max_jump_at_sec)
        .def_readonly("platform", &AdvancedAnalysisResult::platform);

    // Functions (NumPy-friendly)
    m.def(
        "analyze_stereo",
        &analyze_numpy,
        py::arg("left"),
        py::arg("right"),
        py::arg("sample_rate"),
        "Analyze stereo audio and return loudness / peak metrics."
    );

    m.def(
        "analyze_advanced_stereo",
        &analyze_advanced_numpy,
        py::arg("left"),
        py::arg("right"),
        py::arg("sample_rate"),
        "Advanced analysis (timeline + insights) for premium UI."
    );
}
