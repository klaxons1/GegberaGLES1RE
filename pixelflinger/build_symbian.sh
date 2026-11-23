#!/usr/bin/env bash
# build_symbian.sh - Build Android Pixelflinger + CodeFlinger + libagl OpenGL ES 1.1 CM
# + Symbian EKA2 EGL 1.1 as a 100% drop-in replacement Symbian OS v9.x ARMv5TE DLL
# (libgles_cm.dll, UID3 0x101fcabd, 175 exact ordinals, Zero WSD).
set -euo pipefail

PF_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${PF_ROOT}/.." && pwd)"
TOOLS_DIR="${SYMBIAN_TOOLS_DIR:-/tmp/symbian_tools}"
GCC_DIR="${TOOLS_DIR}/gcc"
ELF2E32="${TOOLS_DIR}/elf2e32_bin"
BUILD_DIR="/tmp/pixelflinger_symbian_build"
OUT_DIR="${PF_ROOT}/out"

# Bootstrap toolchain if /tmp/symbian_tools is missing
if [[ ! -x "${GCC_DIR}/bin/arm-none-symbianelf-g++" || ! -x "${ELF2E32}" ]]; then
    "${REPO_ROOT}/symbian/build_symbian.sh" >/dev/null
fi

CC="${GCC_DIR}/bin/arm-none-symbianelf-gcc"
CXX="${GCC_DIR}/bin/arm-none-symbianelf-g++"
SIZE="${GCC_DIR}/bin/arm-none-symbianelf-size"
GCC_INTERNAL_INC="$("${CXX}" -print-file-name=include)"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/obj" "${BUILD_DIR}/implibs" "${OUT_DIR}"

echo "[pixelflinger-symbian] Generating Symbian EKA2 OS import libraries (.dso)..."
python3 "${REPO_ROOT}/symbian/gen_implibs.py" "${ELF2E32}" "${BUILD_DIR}/implibs"

ASM_FLAGS=(
    -march=armv5te
    -mtune=arm1136jf-s
    -marm
    -mfloat-abi=soft
    -O2
    -fno-pic
    -fno-PIE
    -fvisibility=hidden
    -D__SYMBIAN32__
    -D__EPOC32__
    -D__MARM__
    -D__EABI__
    -D__ARM_ARCH__=5
)

CXX_FLAGS=(
    -std=gnu++14
    -march=armv5te
    -mtune=arm1136jf-s
    -marm
    -mfloat-abi=soft
    -O2
    -finline-functions
    -fgcse-after-reload
    -fpredictive-commoning
    -funswitch-loops
    -fstrict-aliasing
    -fomit-frame-pointer
    -fno-pic
    -fno-PIE
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fvisibility=hidden
    -fvisibility-inlines-hidden
    -fdata-sections
    -ffunction-sections
    -nostdinc
    -nostdinc++
    -I"${PF_ROOT}/symbian/libc_include"
    -I"${PF_ROOT}/include"
    -I"${PF_ROOT}/libpixelflinger"
    -I"${PF_ROOT}/libagl"
    -I"${REPO_ROOT}/symbian/include"
    -isystem "${GCC_INTERNAL_INC}"
    -DNDEBUG
    -D__SYMBIAN32__
    -D__EPOC32__
    -D__MARM__
    -D__EABI__
    -D__ARM_ARCH__=5
    -DPF_SYMBIAN_DLL_BUILD
    -DGL_GLEXT_PROTOTYPES
    -DEGL_EGLEXT_PROTOTYPES
)

echo "[pixelflinger-symbian] Assembling ARMv5TE startup & scanline/fixed-point kernels..."
ASM_SOURCES=(
    "${PF_ROOT}/symbian/pf_symbian_entry.S"
    "${REPO_ROOT}/symbian/arm_divmod.S"
    "${PF_ROOT}/libpixelflinger/col32cb16blend.S"
    "${PF_ROOT}/libpixelflinger/t32cb16blend.S"
    "${PF_ROOT}/libagl/fixed_asm.S"
    "${PF_ROOT}/libagl/iterators.S"
)

OBJS=()
for s in "${ASM_SOURCES[@]}"; do
    base="$(basename "${s}" .S)"
    obj="${BUILD_DIR}/obj/${base}.o"
    "${CC}" "${ASM_FLAGS[@]}" -c "${s}" -o "${obj}"
    OBJS+=("${obj}")
done

CPP_SOURCES=(
    # Symbian runtime + IEEE-754 ARM EABI softfp + EGL 1.1
    "${PF_ROOT}/symbian/pf_symbian_rt.cpp"
    "${PF_ROOT}/symbian/aeabi_softfp.cpp"
    "${PF_ROOT}/symbian/egl_symbian.cpp"
    # libpixelflinger + codeflinger + tinyutils
    "${PF_ROOT}/libpixelflinger/buffer.cpp"
    "${PF_ROOT}/libpixelflinger/clear.cpp"
    "${PF_ROOT}/libpixelflinger/fixed.cpp"
    "${PF_ROOT}/libpixelflinger/format.cpp"
    "${PF_ROOT}/libpixelflinger/picker.cpp"
    "${PF_ROOT}/libpixelflinger/pixelflinger.cpp"
    "${PF_ROOT}/libpixelflinger/raster.cpp"
    "${PF_ROOT}/libpixelflinger/scanline.cpp"
    "${PF_ROOT}/libpixelflinger/trap.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/ARMAssembler.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/ARMAssemblerInterface.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/ARMAssemblerProxy.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/CodeCache.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/GGLAssembler.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/blending.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/load_store.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/texturing.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/tinyutils/SharedBuffer.cpp"
    "${PF_ROOT}/libpixelflinger/codeflinger/tinyutils/VectorImpl.cpp"
    # libagl + OpenGL ES 1.1 full 144-ordinal frontend
    "${PF_ROOT}/libagl/BufferObjectManager.cpp"
    "${PF_ROOT}/libagl/TextureObjectManager.cpp"
    "${PF_ROOT}/libagl/TokenManager.cpp"
    "${PF_ROOT}/libagl/Tokenizer.cpp"
    "${PF_ROOT}/libagl/array.cpp"
    "${PF_ROOT}/libagl/fp.cpp"
    "${PF_ROOT}/libagl/light.cpp"
    "${PF_ROOT}/libagl/matrix.cpp"
    "${PF_ROOT}/libagl/mipmap.cpp"
    "${PF_ROOT}/libagl/primitives.cpp"
    "${PF_ROOT}/libagl/state.cpp"
    "${PF_ROOT}/libagl/texture.cpp"
    "${PF_ROOT}/libagl/vertex.cpp"
    "${PF_ROOT}/libagl/gles11_ext.cpp"
)

echo "[pixelflinger-symbian] Compiling ${#CPP_SOURCES[@]} C++ sources (-march=armv5te -mtune=arm1136jf-s -O2)..."
for src in "${CPP_SOURCES[@]}"; do
    rel="${src#${PF_ROOT}/}"
    flat="${rel//\//_}"
    obj="${BUILD_DIR}/obj/${flat%.cpp}.o"
    "${CXX}" "${CXX_FLAGS[@]}" -c "${src}" -o "${obj}"
    OBJS+=("${obj}")
done

echo "[pixelflinger-symbian] Linking Symbian EABI ELF with Zero-WSD & Zero-.init_array enforcement..."
DSOS=(
    "${BUILD_DIR}/implibs/euser{000a0000}.dso"
    "${BUILD_DIR}/implibs/ws32{000a0000}.dso"
    "${BUILD_DIR}/implibs/fbscli{000a0000}.dso"
    "${BUILD_DIR}/implibs/cone{000a0000}.dso"
    "${BUILD_DIR}/implibs/gdi{000a0000}.dso"
    "${BUILD_DIR}/implibs/hal{000a0000}.dso"
)

LIBGCC_A="$("${CXX}" -march=armv5te -marm -mfloat-abi=soft -print-libgcc-file-name)"

"${CXX}" \
    -march=armv5te -mtune=arm1136jf-s -marm -mfloat-abi=soft \
    -O2 -fno-pic -fno-exceptions -fno-rtti \
    -nostdlib -shared \
    -Wl,-T,"${PF_ROOT}/symbian/pf_symbian_dll.ld" \
    -Wl,--entry,_E32Dll \
    -Wl,--version-script="${PF_ROOT}/symbian/pf_libgles_cm.ver" \
    -Wl,-Bsymbolic \
    -Wl,--gc-sections \
    -Wl,--no-undefined \
    -Wl,-Map="${BUILD_DIR}/libgles_cm.map" \
    "${OBJS[@]}" \
    "${DSOS[@]}" \
    "${LIBGCC_A}" \
    -o "${BUILD_DIR}/libgles_cm.sym"

"${SIZE}" "${BUILD_DIR}/libgles_cm.sym"

echo "[pixelflinger-symbian] Running elf2e32 -> libgles_cm.dll & libgles_cm.dso..."
"${ELF2E32}" \
    --targettype=DLL \
    --elfinput="${BUILD_DIR}/libgles_cm.sym" \
    --definput="${REPO_ROOT}/symbian/libgles_cm.def" \
    --output="${OUT_DIR}/libgles_cm.dll" \
    --dso="${OUT_DIR}/libgles_cm{000a0000}.dso" \
    --defoutput="${OUT_DIR}/libgles_cm.def" \
    --libpath="${BUILD_DIR}/implibs" \
    --linkas="libgles_cm{000a0000}.dll" \
    --uid1=0x10000079 \
    --uid2=0x1000008d \
    --uid3=0x101fcabd \
    --sid=0x101fcabd \
    --vid=0x00000000 \
    --version=10.0 \
    --capability=ALL-TCB \
    --fpu=softvfp \
    --uncompressed

cp -f "${OUT_DIR}/libgles_cm{000a0000}.dso" "${OUT_DIR}/libgles_cm.dso"

python3 - "${OUT_DIR}" "${PF_ROOT}" <<'PY'
import os, sys, zipfile
out_dir, pf_root = sys.argv[1], sys.argv[2]
zip_path = os.path.join(out_dir, "Pixelflinger-GLES1-Symbian-EKA2-ARMv5.zip")
with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(os.path.join(pf_root, "README.md"), "README.md")
    zf.write(os.path.join(out_dir, "libgles_cm.dll"), "sys/bin/libgles_cm.dll")
    zf.write(os.path.join(out_dir, "libgles_cm.dso"), "epoc32/release/armv5/lib/libgles_cm.dso")
    zf.write(os.path.join(out_dir, "libgles_cm{000a0000}.dso"), "epoc32/release/armv5/lib/libgles_cm{000a0000}.dso")
    zf.write(os.path.join(out_dir, "libgles_cm.def"), "epoc32/include/def/eabi/libgles_cmU.def")
    zf.write(os.path.join(pf_root, "include/GLES/gl.h"), "epoc32/include/GLES/gl.h")
    zf.write(os.path.join(pf_root, "include/GLES/glext.h"), "epoc32/include/GLES/glext.h")
    zf.write(os.path.join(pf_root, "include/EGL/egl.h"), "epoc32/include/EGL/egl.h")
    zf.write(os.path.join(pf_root, "include/EGL/eglext.h"), "epoc32/include/EGL/eglext.h")
    zf.write(os.path.join(pf_root, "include/EGL/eglplatform.h"), "epoc32/include/EGL/eglplatform.h")
    zf.write(os.path.join(pf_root, "include/KHR/khrplatform.h"), "epoc32/include/KHR/khrplatform.h")
PY

echo "[pixelflinger-symbian] Build complete:"
ls -lh "${OUT_DIR}/libgles_cm.dll" "${OUT_DIR}/libgles_cm.dso" "${OUT_DIR}/Pixelflinger-GLES1-Symbian-EKA2-ARMv5.zip"
