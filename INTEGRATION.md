# Integrating HeadroomDSP back into the Headroom desktop app

The extraction is non-destructive; `Headroom.zip` has not been modified.

## Current compatibility

- `headroom::analyze_stereo()` and `headroom::analyze_advanced_stereo()` function signatures are preserved.
- The C ABI function names and the `HRResult` layout are preserved.
- The optional Python module name remains `headroom_dsp` and the binding classes retain their existing field names.
- Legacy `AdvancedAnalysisResult::true_peak_dbfs` remains a **sample-peak** timeline. Do not display it as dBTP.
- Short clips (0.4–3 s) now yield an empty advanced 3 s timeline rather than a misleading partial short-term window.

## Example migration

1. Develop and tag a validated standalone HeadroomDSP release.
2. In the Headroom build, reference the tagged release (e.g. via CMake `FetchContent`, a system-installed `find_package(HeadroomDSP)`, or a Git submodule).
3. Replace compilation of the copies in `Headroom/cpp/headroom_dsp/` with linkage to `HeadroomDSP::core` (and `HeadroomDSP::c` if using C ABI).
4. If the app imports the Python extension directly, build the `headroom_dsp` pybind11 module and package it as before.
5. Run original UI, Python reporting and native sidecar regression tests. The app may contain additional audio pipelines that do **not** use this extracted C++ engine.

**Do not blindly overwrite the Headroom DSP files with this v0.1.0 implementation.** It needs independent standards conformance and application regression testing before production use.
