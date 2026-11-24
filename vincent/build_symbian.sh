#!/usr/bin/env bash
# Vincent GLES1 Common Profile, EKA2 ARMv5TE, zero WSD, 175 ROM ordinals.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
V="$ROOT/vincent"
TOOLS="${SYMBIAN_TOOLS_DIR:-/tmp/symbian_tools}"
if [[ ! -x "$TOOLS/gcc/bin/arm-none-symbianelf-g++" || ! -x "$TOOLS/elf2e32_bin" ]]; then
    "$ROOT/symbian/build_symbian.sh" > /tmp/vincent_bootstrap.log 2>&1 || { cat /tmp/vincent_bootstrap.log; exit 1; }
fi
CXX="$TOOLS/gcc/bin/arm-none-symbianelf-g++"
CC="$TOOLS/gcc/bin/arm-none-symbianelf-gcc"
B="${VINCENT_BUILD_DIR:-/tmp/vincent_symbian_build}"
O="$V/out"
mkdir -p "$B/obj" "$B/implibs" "$O"
python3 "$ROOT/symbian/gen_implibs.py" "$TOOLS/elf2e32_bin" "$B/implibs"
COMMON=(-march=armv5te -mtune=arm1136jf-s -marm -mfloat-abi=soft -O2
    -fno-pic -fno-PIE -fvisibility=hidden -D__SYMBIAN32__ -D__EPOC32__ -D__MARM__ -D__EABI__)
FLAGS=("${COMMON[@]}" -std=gnu++14 -fno-strict-aliasing -fwrapv -fno-exceptions -fno-rtti
    -fno-threadsafe-statics -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables
    -fdata-sections -ffunction-sections -nostdinc -nostdinc++ -DNDEBUG -DEGL_NO_COMPILE
    -I"$V/platform" -I"$V/upstream/include" -I"$V/upstream/src" -I"$V/upstream/src/arm"
    -I"$ROOT/pixelflinger/symbian/libc_include" -I"$ROOT/symbian/include"
    -isystem "$("$CXX" -print-file-name=include)")
OBJS=()
for src in "$ROOT/pixelflinger/symbian/pf_symbian_entry.S" "$ROOT/symbian/arm_divmod.S"; do
    obj="$B/obj/$(basename "${src%.S}").o"
    "$CC" "${COMMON[@]}" -c "$src" -o "$obj"; OBJS+=("$obj")
done
SOURCES=("$V/platform/Surface.cpp" "$V/platform/FunctionCache.cpp" "$V/platform/egl.cpp"
    "$V/platform/compat.cpp" "$V/platform/runtime.cpp" "$ROOT/pixelflinger/symbian/aeabi_softfp.cpp")
for src in "$V"/upstream/src/*.cpp; do
    case "$src" in */RasterizerPieces.cpp|*/Display.cpp|*/arithmetic.cpp) continue;; esac
    SOURCES+=("$src")
done
for src in "${SOURCES[@]}"; do
    echo "[Vincent] $(basename "$src")"
    obj="$B/obj/$(basename "${src%.cpp}").o"
    "$CXX" "${FLAGS[@]}" -c "$src" -o "$obj"; OBJS+=("$obj")
done
DSOS=()
for dll in euser ws32 fbscli cone gdi hal; do DSOS+=("$B/implibs/$dll{000a0000}.dso"); done
LIBGCC="$("$CXX" -march=armv5te -marm -mfloat-abi=soft -print-libgcc-file-name)"
"$CXX" "${COMMON[@]}" -nostdlib -shared -Wl,-T,"$ROOT/pixelflinger/symbian/pf_symbian_dll.ld" \
    -Wl,--entry,_E32Dll -Wl,--version-script="$ROOT/pixelflinger/symbian/pf_libgles_cm.ver" \
    -Wl,-Bsymbolic -Wl,--gc-sections -Wl,--no-undefined -Wl,-Map="$B/libgles_cm.map" \
    "${OBJS[@]}" "${DSOS[@]}" "$LIBGCC" -o "$B/libgles_cm.sym"
"$TOOLS/gcc/bin/arm-none-symbianelf-size" "$B/libgles_cm.sym"
"$TOOLS/elf2e32_bin" --targettype=DLL --elfinput="$B/libgles_cm.sym" \
    --definput="$ROOT/symbian/libgles_cm.def" --output="$O/libgles_cm.dll" \
    --dso="$O/libgles_cm{000a0000}.dso" --defoutput="$O/libgles_cm.def" \
    --libpath="$B/implibs" --linkas="libgles_cm{000a0000}.dll" \
    --uid1=0x10000079 --uid2=0x1000008d --uid3=0x101fcabd --sid=0x101fcabd \
    --vid=0 --version=10.0 --capability=ALL-TCB --fpu=softvfp --uncompressed
cp "$O/libgles_cm{000a0000}.dso" "$O/libgles_cm.dso"
python3 "$V/package.py"
echo "[Vincent] Ready: $O/libgles_cm.dll"
