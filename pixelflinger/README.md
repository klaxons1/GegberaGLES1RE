# Android Pixelflinger + Codeflinger + libagl for Symbian OS EKA2 (ARMv5TE)

Drop-in Symbian OS v9.x / S60 3rd & 5th Edition / Symbian^3 (`EKA2`) OpenGL ES 1.1 Common-Lite & Common (`libgles_cm.dll`) implementation powered by Android's **`libpixelflinger`**, **`codeflinger` ARMv5TE runtime JIT scanline compiler**, and **`libagl`** software 3D pipeline.

## Binary & ABI Summary

| Property | Value |
|---|---|
| Target filename | `sys\bin\libgles_cm.dll` (`libgles_cm{000a0000}.dll`) |
| Symbian UIDs | `0x10000079` / `0x1000008d` / `0x101fcabd` (`SID = 0x101fcabd`) |
| CPU / ISA | `ARMv5TE` (`-march=armv5te -mtune=arm1136jf-s -marm -mfloat-abi=soft`) |
| Export table | **175 exact ordinals** matching Symbian ROM `libgles_cm.def` (29 EGL 1.1 + 146 OpenGL ES 1.1 & OES) |
| Writable Static Data (WSD) | **0 bytes** (`Data Size = 0`, `Bss Size = 0`, zero `.init_array`) |
| `GL_VENDOR` / `EGL_VENDOR` | `"klaxons"` |
| `GL_RENDERER` | `"Android PixelFlinger 1.4"` |
| `GL_VERSION` | `"OpenGL ES-CM 1.1"` |
| ROM DLL imports | 5 OS ROM DLLs only: `euser`, `ws32`, `fbscli`, `cone`, `hal` |

## Key Architecture Highlights

1. **Zero-WSD Symbian EKA2 Architecture**:
   - All per-thread EGL/GL state, expandable `UserHeap::ChunkHeap` (up to 32 MB, bypassing S60's 1 MB default thread heap cap), and JIT code cache state live in Symbian DLL TLS (`Dll::Tls` / `Dll::SetTls`), torn down cleanly on thread/process detach.
   - All C++ vtables and const relocation tables (`.data.rel.ro*`) reside in `ER_RO` with linker assertions enforcing zero `.data`, zero `.bss`, and zero `.init_array`.
2. **Codeflinger ARMv5TE Runtime JIT (`RChunk::CreateLocalCode`)**:
   - Synthesizes specialized 32-bit ARMv5TE scanline inner loops (`SMULWB`, `SMLAWB`, `SMULBB`, `PLD`) at runtime for active depth, fog, shading, blending, and multi-texture states into a 256 KB local executable code chunk (`RChunk::CreateLocalCode`), flushing the split ARM9/ARM11 I-cache/D-cache via `User::IMB_Range` (`euser` ordinal 1507 / 1363).
   - In-place arena block shrinking preserves assembled branch target addresses, with `mCount(0)` `sp<Assembly>` lifecycle fixing upstream cache eviction leaks.
3. **Full 175-Ordinal OpenGL ES 1.1 CM + EGL 1.1 Feature Set**:
   - Vertex & index buffer objects (`GL_ARRAY_BUFFER`, `GL_ELEMENT_ARRAY_BUFFER`), auto-mipmapping (`GL_GENERATE_MIPMAP`), ETC1 (`GL_ETC1_RGB8_OES`) & paletted compressed textures, `GL_OES_draw_texture`, `GL_OES_matrix_get`, `GL_OES_query_matrix`, `GL_OES_point_sprite`, `GL_OES_point_size_array`, user clip planes, and full dynamic ES 1.1 state queries.
   - Native Symbian `RWindow` (`CWsBitmap` + `CWindowGc::BitBlt` with FBS heap lock/unlock discipline and live S60 portrait/landscape resize handling), `CFbsBitmap` pixmap surfaces (`eglCopyBuffers`), and Pbuffer render-to-texture (`eglBindTexImage` / `eglReleaseTexImage`).
   - Self-contained bit-exact IEEE-754 ARM EABI software floating-point (`aeabi_softfp.cpp`) and CLZ-accelerated ARMv5T integer division (`arm_divmod.S`) with zero `dfpaeabi.dll` / `drtaeabi.dll` version fragility.

## Building & Testing

```bash
# Build Symbian EKA2 ARMv5TE DLL, DSO import libraries, and SDK zip:
./pixelflinger/build_symbian.sh

# Run the headless 12-stage 3D & ARMv5TE JIT verification suite on Linux:
g++ -std=gnu++14 -O2 -DPF_HEADLESS_UNIX=1 -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES \
  -Ipixelflinger/include -Ipixelflinger/libpixelflinger -Ipixelflinger/libagl \
  pixelflinger/tests/test_pf_headless.cpp pixelflinger/symbian/egl_symbian.cpp \
  pixelflinger/libpixelflinger/*.cpp pixelflinger/libpixelflinger/codeflinger/*.cpp \
  pixelflinger/libpixelflinger/codeflinger/tinyutils/*.cpp pixelflinger/libagl/*.cpp \
  -lpthread -lm -o /tmp/test_pf_headless && /tmp/test_pf_headless
```
