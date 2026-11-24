#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
OUT="${VINCENT_HOST_OUT:-/tmp/vincent_host}"
mkdir -p "$OUT"
SOURCES=()
for s in vincent/upstream/src/*.cpp; do
    case "$s" in */RasterizerPieces.cpp|*/Display.cpp|*/arithmetic.cpp) continue;; esac
    SOURCES+=("$s")
done
g++ -std=gnu++14 -O2 -g -fno-strict-aliasing -fwrapv -fno-exceptions -fno-rtti \
    -DEGL_NO_COMPILE -Ivincent/platform -Ivincent/upstream/include \
    -Ivincent/upstream/src -Ivincent/upstream/src/arm \
    -fsanitize=address "${SOURCES[@]}" vincent/platform/Surface.cpp \
    vincent/platform/FunctionCache.cpp vincent/platform/egl.cpp vincent/platform/compat.cpp \
    vincent/tests/test_headless.cpp -lm -o "$OUT/test_vincent"
"$OUT/test_vincent"
