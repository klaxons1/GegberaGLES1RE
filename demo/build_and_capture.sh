#!/usr/bin/env bash
# Reproducible one-command build & 6-scene OpenGL ES 1.1 + EGL 1.1 screenshot
# showcase running ClassiCube directly on Gerbera (build/libgerbera.a).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEMO_DIR="${REPO_ROOT}/demo"
CC_DIR="${CLASSICUBE_DIR:-/tmp/ClassiCube}"
CC_SEED_VAL="${CC_SEED:-20261122}"

echo "[1/4] Building Gerbera OpenGL ES 1.1 + EGL 1.1 static driver (build/libgerbera.a)..."
make -C "${REPO_ROOT}" CFLAGS="-std=c99 -O3 -DNDEBUG -DHG_NO_ASSERTS -Wall -Wextra -Wno-unused-parameter -Isrc/include" -j"$(nproc)"

if [ ! -d "${CC_DIR}/.git" ]; then
    echo "[2/4] Cloning ClassiCube into ${CC_DIR}..."
    rm -rf "${CC_DIR}"
    git clone --depth 1 https://github.com/ClassiCube/ClassiCube.git "${CC_DIR}"
fi

echo "[2/4] Applying Gerbera EGL 1.1 + OpenGL ES 1.1 window/GL1 patch..."
git -C "${CC_DIR}" checkout -- misc/opengl/GLCommon.h src/Graphics_GL1.c src/_GLShared.h src/Server.c src/Window_Win.c
git -C "${CC_DIR}" apply --ignore-space-change --ignore-whitespace "${DEMO_DIR}/classicube_gles1.patch"
cp "${DEMO_DIR}/Window_Gerbera.c" "${CC_DIR}/src/Window_Gerbera.c"

mkdir -p "${CC_DIR}/texpacks"
cp "${CC_DIR}/misc/cc_textures.zip" "${CC_DIR}/texpacks/default.zip"
cp "${CC_DIR}/misc/cc_textures.zip" "${CC_DIR}/texpacks/classicube.zip"

echo "[3/4] Compiling ClassiCube with OpenGL ES 1.1 + EGL 1.1 linked solely against libgerbera.a..."
make -C "${CC_DIR}" linux -j"$(nproc)" \
    EXTRA_CFLAGS="-O3 -DNDEBUG -I${REPO_ROOT}/src/include -DCC_WIN_BACKEND=99 -DCC_GFX_BACKEND=CC_GFX_BACKEND_GL1 -DCC_BUILD_GLES -DCC_BUILD_EGL -DCC_AUD_BACKEND=CC_AUD_BACKEND_NULL" \
    LIBS="${REPO_ROOT}/build/libgerbera.a -lpthread -ldl -lm"

echo "[4/4] Running ClassiCube (seed=${CC_SEED_VAL}, 960x540) to capture 6 OpenGL ES 1.1 screenshots..."
(
    cd "${CC_DIR}"
    CC_SEED="${CC_SEED_VAL}" CC_DEMO_DIR="${DEMO_DIR}" ./ClassiCube --singleplayer
)

ls -lh "${DEMO_DIR}"/*.png
