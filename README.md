# HeadroomDSP

Independent, MIT-licensed C++17 library for audio loudness, peak, dynamics, and stereo diagnostics.

**Status: v0.1.0, experimental.** The native C++ core has no runtime dependencies. It accepts decoded, planar float samples; callers provide decoding and file I/O. Optional Python bindings use pybind11 and NumPy.

## Features

- K-weighted, gated integrated LUFS (400 ms blocks / 100 ms hop), 95th-percentile momentary and short-term summaries, and EBU-style LRA estimate
- FIR-based 4x inter-sample peak estimation (not linear interpolation)
- Stereo correlation and peak-to-loudness ratio (exposed as `crest_factor_db` for API compatibility)
- Time-aligned 400 ms / 3 s loudness windows, *sample*-peak timeline, dynamic-change hotspots
- Heuristic limiter-stress score and illustrative gain/peak-limited normalization previews
- C++ API, C ABI, optional pybind11 NumPy-compatible module; CMake install/export targets

**Standards caveat:** The K-weighting and gating follow BS.1770 concepts, but this release has **not** passed an official ITU/EBU conformance suite. The 4x FIR reconstruction is a windowed-sinc estimator, **not** the published BS.1770 48-tap reference filter and is not claimed to be certified true-peak metering. Compare with ITU reference waveforms and libebur128 before making broadcast-compliance claims. The LRA implementation is also subject to standards validation. No certification is implied.

## Build

Requires CMake 3.20+ and a C++17 compiler (macOS, Linux, Windows).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/headroomdsp_example  # On Windows: .\\build\\Release\\headroomdsp_example.exe
```

Install and use from another CMake project:

```sh
cmake --install build --prefix /your/install/prefix
```

```cmake
find_package(HeadroomDSP CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE HeadroomDSP::core)
# Or link HeadroomDSP::c for C ABI (link with a C++ linker).
```

```cpp
#include <headroomdsp/analysis.hpp>
#include <vector>
// left and right are equal-size vectors of finite, planar, 32-bit PCM floats.
auto stats = headroom::analyze_stereo(left, right, sample_rate);
// stats.integrated_lufs, stats.true_peak_dbtp, stats.phase_correlation, ...
```

**Python (optional, requires internet/available packages for initial dependency installation):**

```sh
python -m pip install .
python -c 'import headroom_dsp; print(headroom_dsp.__doc__)'
```

```python
import numpy as np
import headroom_dsp
left = np.zeros(48000, dtype=np.float32)
right = left.copy()
result = headroom_dsp.analyze_stereo(left, right, 48000)
print(result.integrated_lufs)  # -inf for silence
```

## API notes

- `analyze_stereo(left,right,sample_rate)` requires equal-size, nonempty, finite vectors, **at least 400 ms** of input, and sample rates 8,000–384,000 Hz. There is no file reader. For mono, duplicate the channel only if you want stereo-equivalent loudness, not genuine mono loudness.
- `momentary_lufs` and `short_term_lufs` fields of the *summary* are 95th percentiles, not the latest instantaneous readings. Short-term is `-inf` when there are no complete 3 s windows.
- `true_peak_dbtp` is the *whole-signal FIR estimate*. The `AdvancedAnalysisResult::true_peak_dbfs` timeline (and C ABI `hr_adv_timeline_peak`) records **ordinary per-hop sample peak, in dBFS**, not true peak; the field name is maintained for API compatibility. Do not label it as dBTP.
- `crest_factor_db` is an existing field name for peak-to-loudness ratio (PLR); it is **not** a conventional sample peak minus RMS crest-factor measurement.
- `limiter_stress_0_100` is a **heuristic**, not a measured limiting amount or a standards-based metric. `platform` contains illustrative -14/-16 LUFS target previews with a -1 dBTP cap, not official policies of any music service.
- Silent programme returns `-inf` loudness and true peak; JSON encoders may need explicit representation for infinities.
- 4x FIR reconstruction incurs extra CPU work. It is an offline/batch-oriented first implementation; not designed for hard real-time audio callbacks.
- C ABI pointers and result handles must remain valid for the duration of calls. Timeline getters write exactly `hr_adv_timeline_len()` doubles into caller-provided buffers; do not provide smaller buffers. Future releases should add checked, length-aware getters for untrusted consumers.

## License

MIT License. Copyright (c) 2025 David Bailey. The library is independently buildable and contains no bundled third-party DSP implementation.

## Reference material

- ITU-R BS.1770-5: https://www.itu.int/rec/R-REC-BS.1770
- EBU R 128: https://tech.ebu.ch/publications/r128
- libebur128 (MIT reference implementation): https://github.com/jiixyj/libebur128

## Planned next steps before v1.0

1. Validate the peak meter against official BS.1770 true-peak vectors; replace the FIR with a formally tested reference design where required.
2. Compare integrated loudness, short-term, momentary, and LRA against libebur128 across 44.1/48/96/192 kHz, silence, gated sections, and multichannel layouts.
3. Improve large-audio performance using prefix-power windows and streaming processing; add allocation and throughput benchmarks.
4. Add genuine mono and multichannel APIs, precise C ABI buffer-length contracts, and versioned ABI guarantees.
