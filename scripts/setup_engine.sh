#!/usr/bin/env bash
# Copies chaupar's in-house axmol 2.2.1 fork into ./axmol (gitignored). Idempotent.
set -euo pipefail
cd "$(dirname "$0")/.."
SRC="chaupar/axmol"
if [ ! -f "$SRC/core/axmolver.h" ]; then
	echo "ERROR: $SRC not found. Clone chaupar (with submodules) into ./chaupar first." >&2
	exit 1
fi
# Exclude .git: chaupar's axmol/.git is a gitlink file (gitdir: ../.git/modules/axmol) that would be broken here.
rsync -a --delete --exclude '/.git' --exclude '/tests/' --exclude '/docs/' "$SRC/" axmol/
[ -f axmol/core/axmolver.h ] || { echo "ERROR: copy failed" >&2; exit 1; }
[ -d axmol/3rdparty/zlib/_x ] || { echo "ERROR: prebuilt deps (3rdparty/*/_x) missing; offline build will fail" >&2; exit 1; }
echo "Engine ready at ./axmol"
