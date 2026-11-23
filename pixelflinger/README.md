# Android Pixelflinger + libagl for Symbian OS EKA2 (ARMv5TE, Non-JIT C++)

Drop-in Symbian OS v9.x / S60 3rd & 5th Edition / Symbian^3 (`EKA2`) OpenGL ES 1.1 Common-Lite & Common (`libgles_cm.dll`) implementation powered by Android's **`libpixelflinger`** (pure verified C++ scanline rasterizer, JIT disabled) and **`libagl`** software 3D pipeline.

## Binary & ABI Summary

| Property | Value |
|---|---|
| Target filename | `sys\bin\libgles_cm.dll` (`libgles_cm{000a0000}.dll`) |
| File / Code size | `144,708` bytes (`0x23544`) / `142,404` bytes (`0x22C44`) |
| Symbian UIDs | `0x10000079` / `0x1000008d` / `0x101fcabd` (`SID = 0x101fcabd`) |
| CPU / ISA | `ARMv5TE` (`-march=armv5te -mtune=arm1136jf-s -marm -mfloat-abi=soft`) |
| Export table | **175 exact ordinals** matching Symbian ROM `libgles_cm.def` (29 EGL 1.1 + 146 OpenGL ES 1.1 & OES) |
| Writable Static Data (WSD) | **0 bytes** (`Data Size = 0`, `Bss Size = 0`, zero `.init_array`) |
| `GL_VENDOR` / `EGL_VENDOR` | `"klaxons"` |
| `GL_RENDERER` | `"Android PixelFlinger 1.4"` |
| `GL_VERSION` | `"OpenGL ES-CM 1.1"` |
| ROM DLL imports | 5 OS ROM DLLs only: `euser`, `ws32`, `fbscli`, `cone`, `hal` |

## Key Architecture Highlights

1. **Pure Non-JIT C++ Scanline Rasterizer (`ANDROID_CODEGEN_C`, `ANDROID_ARM_CODEGEN=0`)**:
   - Runtime JIT (`codeflinger` / `RChunk::CreateLocalCode`) is completely disabled so no executable code chunk permissions or instruction-cache flushes are required on S60 devices or EKA2L1.
   - Verified 1-to-1 against `platform_system_core` (`snapdragon/flight`): compiles both the full generic C++ pixel pipeline (`scanline(context_t*)`, supporting perspective `1/w`, multi-texturing, mipmapping, fog, alpha test, depth test, blending, logic ops, and dithering) and 17 specialized C++ fast-path scanline loops.
2. **Zero-Deadlock Symbian Window & Bitmap Pipeline**:
   - Renders into dedicated 8-byte-aligned RAM color/depth buffers inside a per-thread expandable `UserHeap::ChunkHeap` (up to 32 MB) — never holding `CFbsBitmap::LockHeap` across draw calls or blocking S60 UI/font/bitmap operations.
   - Queries Window Server native display mode (`RWsSession::GetDefModeMaxNumColors`), enforces opaque `EColor16MU` / `EColor64K` `CWsBitmap` blits so `CWindowGc::BitBlt` never treats `alpha = 0` pixels as transparent, and synchronizes `RWindowBase::Size` dynamically before `glViewport`, `glScissor`, `glClear`, and `eglSwapBuffers` (handling S60 `ConstructL()` `(0,0)` -> `SizeChanged()` ordering).
3. **Full 175-Ordinal OpenGL ES 1.1 CM + EGL 1.1 Feature Set**:
   - Native `RGB_565`, `RGBA_8888`, `RGBX_8888`, and `RGBA_4444` colorbuffer configs with full `glCopyTexImage2D`, `glReadPixels`, Pbuffer render-to-texture (`eglBindTexImage` / `eglReleaseTexImage`), `CFbsBitmap` pixmaps (`eglCopyBuffers`), VBOs, auto-mipmapping, ETC1 (`GL_ETC1_RGB8_OES`), and `GL_OES_draw_texture`.
   - Self-contained bit-exact IEEE-754 ARM EABI software floating-point (`aeabi_softfp.cpp`) and 32/64-bit ARM RTABI integer division (`arm_divmod.S`, `__aeabi_uldivmod`, `__aeabi_ldivmod`).

## Building & Testing

```bash
# Build Symbian EKA2 ARMv5TE DLL, DSO import libraries, and SDK zip:
./pixelflinger/build_symbian.sh

# Run the headless non-JIT 3D verification suite on Linux:
g++ -std=gnu++14 -O2 -fno-strict-aliasing -fno-exceptions -fno-rtti \
  -DPF_HEADLESS_UNIX=1 -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES \
  -Ipixelflinger/include -Ipixelflinger/libpixelflinger -Ipixelflinger/libagl \
  pixelflinger/tests/test_pf_headless.cpp pixelflinger/symbian/egl_symbian.cpp \
  pixelflinger/libpixelflinger/*.cpp \
  pixelflinger/libpixelflinger/codeflinger/tinyutils/*.cpp \
  pixelflinger/libagl/*.cpp \
  -lm -o /tmp/test_pf_headless && /tmp/test_pf_headless
```
