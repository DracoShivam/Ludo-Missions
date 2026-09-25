#!/usr/bin/env bash
# Build then run the app binary directly so [LM] stderr logs show in this terminal.
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/build_mac.sh
exec build/bin/LudoMissions/Debug/LudoMissions.app/Contents/MacOS/LudoMissions
