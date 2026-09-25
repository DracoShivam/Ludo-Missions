#!/usr/bin/env bash
# Headless unit tests (pure logic only; does not build the engine).
set -euo pipefail
cd "$(dirname "$0")/.."
# Pin Xcode's SDK/compiler: the Command Line Tools SDK on this machine is newer than Xcode's linker (tapi "unknown architecture").
SDK="$(xcrun --sdk macosx --show-sdk-path)"
cmake -S tests -B build_tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_SYSROOT="$SDK" \
	-DCMAKE_CXX_COMPILER="$(xcrun -f clang++)" > /dev/null
cmake --build build_tests
./build_tests/lm_tests "$@"
