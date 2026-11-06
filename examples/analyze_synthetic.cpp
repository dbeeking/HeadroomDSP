#include "headroomdsp/analysis.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

int main() {
    constexpr int fs = 48000;
    constexpr double pi = 3.14159265358979323846;
    std::vector<float> left(fs * 4), right(fs * 4);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const float s = static_cast<float>(0.2 * std::sin(2 * pi * 1000.0 * i / fs));
        left[i] = s;
        right[i] = s;
    }
    const auto r = headroom::analyze_stereo(left, right, fs);
    std::cout << std::fixed << std::setprecision(2)
              << "Integrated: " << r.integrated_lufs << " LUFS\n"
              << "True peak (FIR estimate): " << r.true_peak_dbtp << " dBTP\n"
              << "LRA: " << r.loudness_range_lu << " LU\n"
              << "Stereo correlation: " << r.phase_correlation << "\n";
}
