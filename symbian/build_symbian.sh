#!/usr/bin/env bash
# build_symbian.sh - Build Gerbera OpenGL ES 1.1 + EGL 1.1 as a native Symbian
# OS v9.x (EKA2 / S60 3rd Ed+) ARM EABI DLL (libgles_cm.dll + libgles_cm.dso).
#
# Uses klaxons1/gcc-symbian and SymbianRevive/elf2e32 on Linux.
# Missing tools are downloaded or built under SYMBIAN_TOOLS_DIR.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_DIR="${SYMBIAN_TOOLS_DIR:-/tmp/symbian_tools}"
GCC_DIR="${TOOLS_DIR}/gcc"
ELF2E32="${TOOLS_DIR}/elf2e32_bin"
BUILD_DIR="/tmp/gerbera_symbian_build"
OUT_DIR="${REPO_ROOT}/symbian/out"

# Bootstrap missing compiler and image-conversion tools.
if [[ ! -x "${GCC_DIR}/bin/arm-none-symbianelf-gcc" ]]; then
    echo "[symbian] Downloading arm-none-symbianelf-gcc 14.2.0..."
    mkdir -p "${TOOLS_DIR}"
    python3 - "${TOOLS_DIR}" <<'PY'
import json, os, subprocess, sys, tarfile, urllib.request

tools_dir = sys.argv[1]
req = urllib.request.Request(
    "https://api.github.com/repos/klaxons1/gcc-symbian/releases",
    headers={"User-Agent": "gerbera-symbian-build"},
)
releases = json.loads(urllib.request.urlopen(req).read().decode())
asset_url = None
for rel in releases:
    for a in rel.get("assets", []):
        if "linux" in a["name"].lower() and a["name"].endswith((".tar.gz", ".tgz", ".tar.xz")):
            asset_url, asset_name = a["browser_download_url"], a["name"]
            break
    if asset_url:
        break
if asset_url:
    archive = os.path.join(tools_dir, asset_name)
    subprocess.check_call(["curl", "--fail", "-L", "-o", archive, asset_url])
else:
    # gcc-symbian distributes its current packages in bin/, not Releases.
    # Use the GitHub API so this also works with restricted build networks.
    import hashlib
    api = "https://api.github.com/repos/klaxons1/gcc-symbian/contents/bin"
    headers = {"User-Agent": "gerbera-symbian-build"}
    entries = json.loads(urllib.request.urlopen(urllib.request.Request(api, headers=headers)).read())
    package = next(e for e in entries if "linux" in e["name"] and e["name"].endswith(".tar.xz"))
    asset_name = package["name"]
    def raw_entry(url):
        return urllib.request.urlopen(urllib.request.Request(url, headers=dict(
            headers, Accept="application/vnd.github.raw+json"))).read()
    archive_data = raw_entry(package["url"])
    checksums = raw_entry(api + "/SHA256SUMS").decode()
    expected = next(line.split()[0] for line in checksums.splitlines() if line.split()[-1].lstrip("*") == asset_name)
    assert hashlib.sha256(archive_data).hexdigest() == expected, "Toolchain checksum mismatch"
    archive = os.path.join(tools_dir, asset_name)
    with open(archive, "wb") as out:
        out.write(archive_data)
extract_dir = os.path.join(tools_dir, "gcc_extract")
os.makedirs(extract_dir, exist_ok=True)
with tarfile.open(archive) as tf:
    tf.extractall(extract_dir)
entries = os.listdir(extract_dir)
# Current archives contain gcc/ alongside gcc-fp-src/.
root = os.path.join(extract_dir, "gcc")
if not os.path.isdir(root):
    root = os.path.join(extract_dir, entries[0]) if len(entries) == 1 else extract_dir
os.rename(root, os.path.join(tools_dir, "gcc"))


# Older Linux packages use C23 aliases unavailable on Debian 12.
# Patch only imported aliases, preserving ELF sizes, and redirect their
# version requirement to the existing GLIBC_2.2.5 version (including hash).
import struct
for sub in ("bin", "arm-none-symbianelf/bin"):
    directory = os.path.join(tools_dir, "gcc", sub)
    if not os.path.isdir(directory):
        continue
    for fn in os.listdir(directory):
        path = os.path.join(directory, fn)
        if not os.path.isfile(path) or os.path.islink(path):
            continue
        data = bytearray(open(path, "rb").read())
        if data[:5] != b"\x7fELF\x02" or b"GLIBC_2.38" not in data:
            continue
        for old, new in [(b"__isoc23_strtoul", b"strtoul"),
                         (b"__isoc23_strtol", b"strtol"),
                         (b"__isoc23_sscanf", b"sscanf")]:
            data = data.replace(old, new.ljust(len(old), b"\0"))
        shoff = struct.unpack_from("<Q", data, 40)[0]
        shsize, shnum = struct.unpack_from("<HH", data, 58)
        sections = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i*shsize) for i in range(shnum)]
        for section in sections:
            if section[1] != 0x6ffffffe:  # SHT_GNU_verneed
                continue
            strings = sections[section[6]]
            string_base = strings[4]
            old_name = b"GLIBC_2.38\0"
            target_name = b"GLIBC_2.2.5\0"
            target_off = data.index(target_name, string_base, string_base + strings[5]) - string_base
            # ELF SysV hash of the replacement version name.
            version_hash = 0
            for ch in target_name[:-1]:
                version_hash = (version_hash << 4) + ch
                high = version_hash & 0xf0000000
                version_hash ^= high >> 24
                version_hash &= ~high
            pos = section[4]
            while True:
                _, count, _, aux, nxt = struct.unpack_from("<HHIII", data, pos)
                apos = pos + aux
                for _ in range(count):
                    name_off, anext = struct.unpack_from("<II", data, apos + 8)
                    if data[string_base + name_off:string_base + name_off + len(old_name)] == old_name:
                        struct.pack_into("<I", data, apos, version_hash)
                        struct.pack_into("<I", data, apos + 8, target_off)
                    apos += anext
                if not nxt:
                    break
                pos += nxt
        with open(path, "wb") as out:
            out.write(data)

PY
fi

if [[ ! -x "${ELF2E32}" ]]; then
    echo "[symbian] Building SymbianRevive/elf2e32..."
    if [[ ! -d "${TOOLS_DIR}/elf2e32" ]]; then
        git clone --depth 1 https://github.com/SymbianRevive/elf2e32.git "${TOOLS_DIR}/elf2e32"
    fi
    g++ -std=c++14 -O2 -D__LINUX__ -fpermissive -w \
        -I"${TOOLS_DIR}/elf2e32/include" -I"${TOOLS_DIR}/elf2e32/source" \
        "${TOOLS_DIR}/elf2e32"/source/*.cpp -o "${ELF2E32}"
fi

CC="${GCC_DIR}/bin/arm-none-symbianelf-gcc"
SIZE="${GCC_DIR}/bin/arm-none-symbianelf-size"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/obj" "${BUILD_DIR}/implibs" "${OUT_DIR}"

# Generate Symbian OS import libraries from the ordinal table.
echo "[symbian] Generating Symbian EKA2 OS import libraries (.dso)..."
python3 "${REPO_ROOT}/symbian/gen_implibs.py" "${ELF2E32}" "${BUILD_DIR}/implibs"

# Compile the freestanding runtime and startup assembly.
COMMON_FLAGS=(
    -std=c99
    -march=armv5t
    -mtune=arm1136jf-s
    -marm
    -mfloat-abi=soft
    -fno-pic
    -fno-PIE
    -fvisibility=hidden
    -fdata-sections
    -ffunction-sections
    -fno-builtin-printf
    -fno-builtin-fprintf
    -fno-strict-aliasing
    -fno-asynchronous-unwind-tables
    -funwind-tables
    -fomit-frame-pointer
    -DNDEBUG
    -D__SYMBIAN32__
    -D__EPOC32__
    -D__MARM__
    -D__EABI__
    -I"${REPO_ROOT}"
    -I"${REPO_ROOT}/symbian/include"
    -I"${REPO_ROOT}/src/include"
)

echo "[symbian] Assembling EKA2 startup, TLS, setjmp & ARMv5 CLZ divmod..."
"${CC}" -march=armv5t -marm -mfloat-abi=soft -O2 -fno-pic \
    -c "${REPO_ROOT}/symbian/symbian_entry.S" -o "${BUILD_DIR}/obj/symbian_entry.o"
"${CC}" -march=armv5t -marm -mfloat-abi=soft -O2 -fno-pic \
    -c "${REPO_ROOT}/symbian/arm_divmod.S" -o "${BUILD_DIR}/obj/arm_divmod.o"
"${CC}" "${COMMON_FLAGS[@]}" -O2 \
    -c "${REPO_ROOT}/symbian/symbian_rt.c" -o "${BUILD_DIR}/obj/symbian_rt.o"

# Combine the renderer sources for whole-program optimization.
UNITY_C="${BUILD_DIR}/gerbera_symbian_unity.c"
{
    echo '#include "GLES/gl.h"'
    echo '#include "GLES/egl.h"'
    while IFS= read -r src; do
        if [[ "$(basename "${src}")" == "hcX86Backend.c" ]]; then
            continue
        fi
        rel="${src#${REPO_ROOT}/}"
        echo "#include \"${rel}\""
    done < <(find "${REPO_ROOT}/src" -name '*.c' | sort)
} > "${UNITY_C}"

echo "[symbian] Compiling Gerbera Whole-Program Unity TU (-march=armv5t -mtune=arm1136jf-s -O2 -finline-functions -fgcse-after-reload -fpredictive-commoning -funswitch-loops -fwhole-program)..."
"${CC}" "${COMMON_FLAGS[@]}" -O2 -finline-functions -fgcse-after-reload -fpredictive-commoning -funswitch-loops -fwhole-program \
    -c "${UNITY_C}" -o "${BUILD_DIR}/obj/gerbera_unity.o"

# Reject writable static data and unresolved symbols at link time.
echo "[symbian] Linking Symbian EABI ELF..."
DSOS=(
    "${BUILD_DIR}/implibs/euser{000a0000}.dso"
    "${BUILD_DIR}/implibs/drtaeabi{000a0000}.dso"
    "${BUILD_DIR}/implibs/ws32{000a0000}.dso"
    "${BUILD_DIR}/implibs/fbscli{000a0000}.dso"
    "${BUILD_DIR}/implibs/cone{000a0000}.dso"
    "${BUILD_DIR}/implibs/gdi{000a0000}.dso"
    "${BUILD_DIR}/implibs/hal{000a0000}.dso"
)

"${CC}" \
    -march=armv5t -mtune=arm1136jf-s -marm -mfloat-abi=soft \
    -O2 -fno-pic \
    -nostdlib -shared \
    -Wl,-T,"${REPO_ROOT}/symbian/symbian_dll.ld" \
    -Wl,--entry,_E32Dll \
    -Wl,--version-script="${REPO_ROOT}/symbian/libgles_cm.ver" \
    -Wl,-Bsymbolic \
    -Wl,--gc-sections \
    -Wl,--no-undefined \
    -Wl,-Map="${BUILD_DIR}/libgles_cm.map" \
    "${BUILD_DIR}/obj/symbian_entry.o" \
    "${BUILD_DIR}/obj/arm_divmod.o" \
    "${BUILD_DIR}/obj/symbian_rt.o" \
    "${BUILD_DIR}/obj/gerbera_unity.o" \
    "${DSOS[@]}" \
    -o "${BUILD_DIR}/libgles_cm.sym"

"${SIZE}" "${BUILD_DIR}/libgles_cm.sym"

# Convert the ELF shared object to an E32Image and import library.
echo "[symbian] Running elf2e32 -> libgles_cm.dll & libgles_cm.dso..."
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

python3 - "${OUT_DIR}" "${REPO_ROOT}" <<'PY'
import os, sys, zipfile
out_dir, repo_root = sys.argv[1], sys.argv[2]
zip_path = os.path.join(out_dir, "Gerbera-GLES1-Symbian-EKA2-ARMv5.zip")
with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(os.path.join(out_dir, "libgles_cm.dll"), "sys/bin/libgles_cm.dll")
    zf.write(os.path.join(out_dir, "libgles_cm.dso"), "epoc32/release/armv5/lib/libgles_cm.dso")
    zf.write(os.path.join(out_dir, "libgles_cm{000a0000}.dso"), "epoc32/release/armv5/lib/libgles_cm{000a0000}.dso")
    zf.write(os.path.join(out_dir, "libgles_cm.def"), "epoc32/include/def/eabi/libgles_cmU.def")
    zf.write(os.path.join(repo_root, "src/include/GLES/gl.h"), "epoc32/include/GLES/gl.h")
    zf.write(os.path.join(repo_root, "src/include/GLES/egl.h"), "epoc32/include/GLES/egl.h")
PY

echo "[symbian] Build complete:"
ls -lh "${OUT_DIR}/libgles_cm.dll" "${OUT_DIR}/libgles_cm.dso" "${OUT_DIR}/Gerbera-GLES1-Symbian-EKA2-ARMv5.zip"
