#!/usr/bin/env bash
# Build Nokia's M3G 1.1 core (m3gcore11, EPL-1.0) from
# SymbianSource/oss.FCL.sf.os.graphics and run it on host PixelFlinger.
#
# The m3gcore sources are fetched at test time (not vendored). m3gcore is an
# ILP32 code base that stores pointers in 32-bit handles; the host build is a
# non-PIE x86-64 binary whose heap is kept below 2 GB (low4g_malloc.c), and
# m3g_defs.h's ILP32 layout compile-time asserts are disabled in the scratch
# copy only.
#
# Usage: pixelflinger/tests/m3g/run_m3gcore_host.sh [--asan] [--16bpp]
#   M3G_SRC=/path/to/oss.FCL.sf.os.graphics  reuse an existing checkout
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PF="$(cd "$HERE/../.." && pwd)"
WORK="${M3G_WORK:-/tmp/pf_m3gcore}"
SRC="${M3G_SRC:-$WORK/oss.FCL.sf.os.graphics}"
REPO=https://github.com/SymbianSource/oss.FCL.sf.os.graphics.git

SAN=""; EXTRA=""
for a in "$@"; do
  case "$a" in
    --asan)  SAN="-fsanitize=address,undefined -fno-omit-frame-pointer" ;;
    --16bpp) EXTRA="-DPF_FRAMEBUFFER_16BPP=1" ;;
    *) echo "unknown option $a" >&2; exit 2 ;;
  esac
done
# ASan's allocator would bypass low4g_malloc and hand out >2 GB pointers.
if [ -n "$SAN" ]; then
  SAN="-fsanitize=undefined -fno-omit-frame-pointer"
  echo "note: --asan uses UBSan only (ASan's heap cannot satisfy 32-bit M3G handles)"
fi

mkdir -p "$WORK"
if [ ! -f "$SRC/m3g/m3gcore11/src/m3g_core.c" ]; then
  rm -rf "$SRC"
  git clone -q --depth 1 --filter=blob:none --sparse "$REPO" "$SRC"
  git -C "$SRC" sparse-checkout set m3g/m3gcore11 graphics_plat/m3g_core_api
fi
echo "m3gcore11: $(git -C "$SRC" rev-parse --short HEAD 2>/dev/null || echo local)"

M3G="$WORK/m3gcore11"
rm -rf "$M3G"
cp -r "$SRC/m3g/m3gcore11" "$M3G"
mkdir -p "$M3G/inc/M3G"
cp "$SRC/graphics_plat/m3g_core_api/inc/M3G/m3g_core.h" "$M3G/inc/M3G/"
# LP64 host: neutralize ILP32 struct-layout compile-time assertions.
sed -i -E \
  -e 's/^(#define M3G_CT_ASSERT\(a\)).*/\1 struct __M3G_UNIQUE_NAME { int unused; }/' \
  -e 's/^(#define M3G_CT_ASSERT1\(a\)).*/\1 struct __M3G_UNIQUE_NAME1 { int unused; }/' \
  -e 's/^(#define M3G_CT_ASSERT2\(a\)).*/\1 struct __M3G_UNIQUE_NAME2 { int unused; }/' \
  -e 's|^M3G_CT_ASSERT\(sizeof\(M3Guint\) >= sizeof\(void\*\)\);|/* host: heap kept below 2 GB */|' \
  "$M3G/inc/m3g_defs.h"

CDEFS="-DPF_HEADLESS_UNIX=1 -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES $EXTRA"
OBJ="$WORK/obj"; [ -z "$EXTRA" ] || OBJ="$WORK/obj_16"
mkdir -p "$OBJ"
COMMON="-O1 -g -fno-pie -fno-strict-aliasing $SAN"

# m3gcore engine (unity build). M3G_DEBUG enables M3G_ASSERT, heap tracking and
# M3G_ASSERT_GL (glGetError()==GL_NO_ERROR after GL operations).
gcc -std=gnu99 $COMMON -w -DM3G_TARGET_GENERIC -DM3G_DEBUG $CDEFS \
    -I"$M3G/inc" -I"$PF/include" -c "$M3G/src/m3g_core.c" -o "$OBJ/m3g_core.o"
gcc -std=gnu99 $COMMON -Wall -I"$M3G/inc" -I"$PF/include" $CDEFS \
    -c "$HERE/m3g_host_glue.c" -o "$OBJ/m3g_host_glue.o"
gcc -std=gnu99 $COMMON -Wall -c "$HERE/low4g_malloc.c" -o "$OBJ/low4g_malloc.o"
gcc -std=gnu99 $COMMON -Wall -Wno-unused-function -I"$M3G/inc" -I"$PF/include" $CDEFS \
    -c "$HERE/test_m3gcore_pf.c" -o "$OBJ/test_m3gcore_pf.o"

# PixelFlinger + libagl + EGL (same sources as the README host test)
PFOBJ="$OBJ/pf"; mkdir -p "$PFOBJ"
PFSRC=$(ls "$PF"/symbian/egl_symbian.cpp "$PF"/libpixelflinger/*.cpp \
           "$PF"/libpixelflinger/codeflinger/*.cpp \
           "$PF"/libpixelflinger/codeflinger/tinyutils/*.cpp "$PF"/libagl/*.cpp)
pids=()
for f in $PFSRC; do
  o="$PFOBJ/$(echo "$f" | sed "s|$PF/||; s|/|_|g; s|\.cpp$|.o|")"
  g++ -std=gnu++14 $COMMON -fno-exceptions -fno-rtti $CDEFS \
      -I"$PF/include" -I"$PF/libpixelflinger" -I"$PF/libagl" -c "$f" -o "$o" &
  pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

g++ -no-pie $SAN "$OBJ"/test_m3gcore_pf.o "$OBJ"/m3g_core.o "$OBJ"/m3g_host_glue.o \
    "$OBJ"/low4g_malloc.o "$PFOBJ"/*.o -lm -lpthread -o "$WORK/test_m3gcore_pf"
"$WORK/test_m3gcore_pf"
