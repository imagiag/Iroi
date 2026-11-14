#!/bin/sh
# Builds a numbered Iroi patch named after the feature it adds, and keeps it:
#   tools/release.sh "feature label" ["what changed"]
#       ->  releases/iroi-v<N>-<feature-label>.syx  + a line in releases/RELEASES.md
# Runs the host tests first (V/Oct tracking, resonator smoke test, original mode
# identical to stock) and refuses to build if they fail. N counts up from
# releases/BUILD; the patch is named Iroi_tw_v<N> on the module. When the code
# is committed the build is also tagged iroi-vN. Commit releases/ afterwards.
set -e
cd "$(dirname "$0")/.."
[ -n "$1" ] || { echo "usage: tools/release.sh \"feature label\" [\"what changed\"]"; exit 1; }
DESC=${2:-$1}
TOOLCHAIN=/Users/loop/Local/AI/Claude/SynthUx/toolchain/arm-gnu-toolchain-13.3.rel1-darwin-arm64-arm-none-eabi/bin
command -v arm-none-eabi-gcc >/dev/null || PATH="$TOOLCHAIN:$PATH"

make -C test >/dev/null || { echo "host tests failed (make -C test)"; exit 1; }
make -C test regression >/dev/null || { echo "original mode differs from stock (make -C test regression)"; exit 1; }

mkdir -p releases
N=$(( $(cat releases/BUILD 2>/dev/null || echo 0) + 1 ))
SLUG=$(printf '%s' "$1" | tr 'A-Z' 'a-z' | sed -e 's/[^a-z0-9]\{1,\}/-/g' -e 's/^-//' -e 's/-$//' | cut -c1-32)
NAME="iroi-v$N-$SLUG"
COMMIT=$(git rev-parse --short HEAD)
DIRTY=$(git diff --quiet HEAD -- . ':!releases' 2>/dev/null || echo "+changes")

# OwlProgram predates GCC 13's freestanding <string>: build the C++ as hosted
# (it is still linked with -nostdlib, nothing from the hosted library is used).
OWLFLAGS="-fno-rtti -fno-exceptions -std=gnu++17 -U__STDC_HOSTED__ -D__STDC_HOSTED__=1 -Wno-builtin-macro-redefined"
OWL="make -C lib/OwlProgram PLATFORM=OWL3 CONFIG=Release PATCHSOURCE=../.. PATCHNAME=Iroi_tw_v$N PATCHCLASS=Iroi_1_0_1Patch PATCHFILE=Iroi_1_0_1Patch.hpp"
mkdir -p build
[ -f lib/OwlProgram/Libraries/libowlprg.a ] || make -C lib/OwlProgram PLATFORM=OWL3 CONFIG=Release CXXFLAGS="$OWLFLAGS" CFLAGS="-std=gnu11 -U__STDC_HOSTED__ -D__STDC_HOSTED__=1 -Wno-builtin-macro-redefined" libs >build/libs.log 2>&1 || true
[ -f lib/OwlProgram/Libraries/libowlprg.a ] || { cat build/libs.log; exit 1; }
$OWL clean >/dev/null
$OWL CXXFLAGS="$OWLFLAGS" patch >build/release.log 2>&1 || { cat build/release.log; exit 1; }
python3 -I tools/owlsyx.py pack lib/OwlProgram/Build/patch.bin "releases/$NAME.syx"

echo "$N" > releases/BUILD
[ -f releases/RELEASES.md ] || printf '# Iroi tweaked patch builds\n\nSend one .syx with the OWL web tool (see TWEAKS.md). To go back to stock, send iroi-stock-1.0.1.syx the same way.\n\n| Build | File | Date | Code | What it adds |\n|---|---|---|---|---|\n' > releases/RELEASES.md
printf '| v%s | %s.syx | %s | %s%s | %s |\n' "$N" "$NAME" "$(date +%Y-%m-%d)" "$COMMIT" "$DIRTY" "$DESC" >> releases/RELEASES.md
# tag the code it was built from, so `git checkout iroi-vN` gets back to it
[ -n "$DIRTY" ] || git tag "iroi-v$N" HEAD
echo "releases/$NAME.syx"
