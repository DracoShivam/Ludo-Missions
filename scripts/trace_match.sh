#!/usr/bin/env bash
# Headless match trace -> JSON. Pure logic, no engine. Usage: scripts/trace_match.sh [seed] [out.json]
set -euo pipefail
cd "$(dirname "$0")/.."
SDK="$(xcrun --sdk macosx --show-sdk-path)"
cmake -S tools -B build_tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_SYSROOT="$SDK" \
	-DCMAKE_CXX_COMPILER="$(xcrun -f clang++)" > /dev/null
cmake --build build_tools > /dev/null
./build_tools/lm_trace "${1:-7}" "${2:-trace.json}" "Content/"
