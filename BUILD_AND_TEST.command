#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is required: brew install cmake"
  exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHEADROOMDSP_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
echo "HEADROOMDSP_BUILD_AND_TEST=PASS"
