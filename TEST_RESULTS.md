# Validation record — v0.1.0

Test environment: Linux x86_64, GCC 14.2, CMake 3.31.

- Release CMake configure and compilation: PASS.
- CTest (`headroomdsp_unit`): PASS.
- Debug build with `-Wall -Wextra -Wpedantic`: PASS (no compiler warnings in build output).
- Debug CTest: PASS.
- Install and downstream `find_package(HeadroomDSP CONFIG REQUIRED)` consumer: PASS.
- Example synthetic audio analysis: integrated loudness -13.97 LUFS; FIR estimated true peak -13.98 dBTP; LRA 0.00 LU; stereo correlation 1.00.
- Optional Python/pybind11 module: **not compiled here** (pybind11 and scikit-build-core were not installed).
- macOS / Windows runtime: **not tested locally**; CI workflow is included but has not yet been run on GitHub.
- Formal ITU BS.1770 / EBU R128 conformance vectors: **not yet run**; no certification claim.
- Original Headroom desktop application regression suite: **not run**; original application was not modified.
