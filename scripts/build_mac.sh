#!/usr/bin/env bash
# Configure + build the macOS app (Debug, arm64, Xcode generator).
set -euo pipefail
cd "$(dirname "$0")/.."
unset AX_ROOT   # ~/.zshrc points AX_ROOT at a DIFFERENT axmol (2.11.4); we use the embedded ./axmol fork
[ -f axmol/core/axmolver.h ] || { echo "Run scripts/setup_engine.sh first" >&2; exit 1; }
# The axmol fork looks files up by BASENAME only: duplicate names in Content/ would silently shadow each other.
DUPES=$(find Content -type f ! -name '.*' -exec basename {} \; | sort | uniq -d)
[ -z "$DUPES" ] || { echo "ERROR: duplicate basenames in Content/: $DUPES" >&2; exit 1; }
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64 "$@"
cmake --build build --config Debug --target LudoMissions -- -quiet
