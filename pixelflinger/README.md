# Android Pixelflinger + CodeFlinger ARMv5TE JIT + libagl for Symbian OS EKA2

Drop-in Symbian OS v9.x / S60 3rd & 5th Edition / Symbian^3 (`EKA2`) OpenGL ES 1.1 Common-Lite & Common (`libgles_cm.dll`) implementation powered by Android's **`libpixelflinger`**, **hardened `codeflinger` ARMv5TE runtime JIT** (aligned with `paralin/platform_system_core` `snapdragon/flight`), ARMv5TE assembly scanline fast-paths (`col32cb16blend.S`, `t32cb16blend.S`), **full compiled-in C++ `scanline(context_t*)` fallback**, and **`libagl`** software 3D pipeline.

## Binary & ABI Summary

| Property | Value |
|---|---|
| Target filename | `sys\bin\libgles_cm.dll` (`libgles_cm{000a0000}.dll`) |
| File / Code size | `229,536` bytes (`0x380A0`) / `226,084` bytes (`0x37324`) |
| Symbian UIDs | `0x10000079` / `0x1000008d` / `0x101fcabd` (`SID = 0x101fcabd`) |
| CPU / ISA | `ARMv5TE` (`-march=armv5te -mtune=arm1136jf-s -marm -mfloat-abi=soft`) |
| Export table | **175 exact ordinals** matching Symbian ROM `libgles_cm.def` (29 EGL 1.1 + 146 OpenGL ES 1.1 & OES) |
| Writable Static Data (WSD) | **0 bytes** (`Data Size = 0`, `Bss Size = 0`, zero `.init_array`) |
| `GL_VENDOR` / `EGL_VENDOR` | `"klaxons"` |
| `GL_RENDERER` | `"Android PixelFlinger 1.4"` |
| `GL_VERSION` | `"OpenGL ES-CM 1.1"` |
| ROM DLL imports | 5 OS ROM DLLs only: `euser`, `ws32`, `fbscli`, `cone`, `hal` |

## Safe ARMv5TE `codeflinger` JIT + Guaranteed C++ Fallback Architecture

1. **Out-of-Band Executable Arena (`RChunk::CreateLocalCode` + `User::IMB_Range`)**:
   - Lazily allocates a per-thread executable code chunk via `RChunk::CreateLocalCode(..., EOwnerProcess)` (`euser@905`), probing `256 KB` -> `128 KB` -> `64 KB` gracefully under low RAM.
   - All allocator bookkeeping (`PfExecSlot slots[256]`) lives **out-of-band in normal `ChunkHeap` RAM**, never inside executable pages, so accidental writes or overruns cannot corrupt free-list headers. Every emitted shader block is 32-byte cacheline-aligned and I-cache synchronized with `User::IMB_Range(base, alignedEnd)` (`euser@667`) before execution.
   - `CodeCache` sizes its watermark to 75% of the actual executable chunk capacity (`192 KB` / `96 KB` / `48 KB`) and proactively LRU-evicts stale shaders both on watermark pressure and inside `Assembly` allocation if fragmentation occurs.
2. **Bounds-Checked, Non-Aborting `ARMAssembler` & `GGLAssembler`**:
   - Every instruction emission in `ARMAssembler` routes through bounds-checked `emit(uint32_t)` (`mLimit` + `mOverflow`) over an `8 KB` (`ASSEMBLY_SCRATCH_SIZE = 8192`) scratch buffer; out-of-range immediates or addressing offsets set `mOverflow = true` (`NO_MEMORY`) instead of calling `User::Invariant()`.
   - Release `LOG_FATAL_IF` assertions during `GGLAssembler`'s multi-pass register-allocator backoff (`mOptLevel = 7 -> 6 -> ... -> 0` on 32-bit `RGBA_8888`, 2-TMU multitexture, fog, and alpha-blended shaders) are compiled out cleanly per `platform_system_core`, and `RegisterFile` exhaustion cleanly triggers lower-opt retry without ever panicking the host Symbian process.
   - Branch fixups resolve string-pooled labels under E32 base relocation via pointer match with `strcmp` fallback.
3. **Always-Compiled-In Full C++ `scanline(context_t*)` Fallback + ARMv5TE ASM Fast-Paths**:
   - Unlike upstream Android builds that replace `scanline(context_t*)` with an empty `{}` stub when `ANDROID_ARM_CODEGEN == 1`, the complete generic C++ pixel pipeline is **always compiled in**. Whenever `RChunk::CreateLocalCode` is denied or a shader cannot be JIT-compiled, `pick_scanline()` seamlessly falls back to `scanline(context_t*)` with zero lost triangles.
   - Hand-tuned ARMv5TE assembly scanlines (`col32cb16blend.S` and `t32cb16blend.S`) accelerate 16-bit `RGB_565` Gouraud and 32-bit-textured alpha blending without JIT overhead.
4. **Zero-Deadlock Symbian Window & Bitmap Pipeline**:
   - Renders into dedicated 8-byte-aligned RAM color/depth buffers inside a per-thread expandable `UserHeap::ChunkHeap` (up to 32 MB) — never holding `CFbsBitmap::LockHeap` across draw calls.
   - Enforces opaque `EColor16MU` / `EColor64K` `CWsBitmap` blits and synchronizes `RWindowBase::Size` before `glViewport`, `glScissor`, `glClear`, and `eglSwapBuffers`.

## Lighting, depth, texture, and point-sprite corrections

- Two-sided lighting reverses back-face normals and relights shared indexed
  vertices; normalization and light attenuation use overflow-safe arithmetic.
- Depth interpolation preserves unsigned Z16 endpoints. Polygon-offset planes
  use signed 64-bit evaluation and per-fragment near/far saturation; only these
  primitives use the C++ pipeline rather than wrapping ARM JIT iterators.
- Texture COMBINE sources, operands, scale queries, and generic fragment
  evaluation are wired through. Supported simple COMBINE configurations still
  lower to the normal JIT paths; other configurations use the C++ fallback.
- Incomplete textures retain the application's enable state and revalidate
  when mip levels are defined. Odd/one-dimensional mipmap generation is bounded.
  As in the upstream PixelFlinger sampler, mip chains are maintained but texture
  sampling uses the base level; this is not a new mip-level selection engine.
- Point sprites honor coordinate replacement, per-vertex point sizes, and
  distance attenuation without overflowing the texture/depth iterators.

Validation: **13/13 host AddressSanitizer tests**, **50,121 IEEE-754 double
helper pairs**, and **8 ARM926EJ-S Unicorn runs** (JIT enabled/denied across
RGB565, RGBA8888, RGBA4444 plus two 64 KB executable-cache stress runs).
These are automated host/emulator results, not physical-phone validation.

## Building & Verification

```bash
# 1. Build Symbian EKA2 ARMv5TE DLL, DSO import libraries, and SDK zip:
./pixelflinger/build_symbian.sh

# 2. Install the ARM emulator (once; activate this venv before testing):
python3 -m venv .venv
. .venv/bin/activate
python -m pip install unicorn

# Run end-to-end ARM926EJ-S (ARMv5TE) Unicorn E32 verification (JIT ON vs JIT OFF + 64KB LRU stress):
python3 pixelflinger/tests/test_e32_armv5_unicorn.py

# Check the freestanding double helpers used by inverse-modelview lighting:
python3 pixelflinger/tests/test_softfp.py

# 3. Run host C++ AddressSanitizer + ARMAssembler/CodeCache + 3D verification suite:
g++ -std=gnu++14 -O2 -g -fsanitize=address -fno-strict-aliasing -fno-exceptions -fno-rtti \
  -DPF_HEADLESS_UNIX=1 -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES \
  -Ipixelflinger/include -Ipixelflinger/libpixelflinger -Ipixelflinger/libagl \
  pixelflinger/tests/test_pf_headless.cpp pixelflinger/symbian/egl_symbian.cpp \
  pixelflinger/libpixelflinger/*.cpp pixelflinger/libpixelflinger/codeflinger/*.cpp \
  pixelflinger/libpixelflinger/codeflinger/tinyutils/*.cpp pixelflinger/libagl/*.cpp \
  -lm -lpthread -o /tmp/test_pf_headless && /tmp/test_pf_headless
```
