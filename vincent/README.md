# Vincent OpenGL ES 1.1 — Symbian EKA2 ARMv5TE port

Source: https://github.com/mcejp/vincent-es (`ogles`, pinned in
`UPSTREAM_REVISION`). Original renderer copyright Hans-Martin Will and other
contributors; see **LICENSE**, retained in the binary package.

This is an alternative to the Gerbera and PixelFlinger implementations, not a
wrapper around either renderer. Vincent's portable fixed-point C++ pipeline is
built with its Common Profile float-to-fixed API layer. The legacy Windows CE
JIT is disabled; this build never executes code from ordinary heap memory.

Build from the repository root:

```sh
./vincent/build_host.sh       # host AddressSanitizer rendering tests
./vincent/build_symbian.sh    # EKA2 DLL, .dso and SDK ZIP
```

Outputs are under `vincent/out/`. The E32 image uses the system GLES DLL UIDs,
175 exact ROM ordinals, ARMv5TE soft-float, and no writable static data or global
C++ constructors. All thread state is held in Dll::Tls / a ChunkHeap.

The native adapter supports window and pbuffer rendering. It uses private RAM
render targets and briefly locks the presentation bitmap only during swaps.
The original eight color/depth/stencil configurations are retained. Context
sharing, pixmap surface creation, and EGL render-to-texture are not supported;
they return EGL errors rather than fake success. No Khronos conformance or
physical-device validation is claimed.

## Validation

The host ASan test covers clear/triangle pixel checks, depth occlusion and
scissored depth clear, texture REPLACE, lighting, exponential fog, lines and
points with disabled varyings, invalid texture units, power underflow,
non-block-aligned surfaces (widths 1–19), RAM resize, EGL errors and deferred
surface/context destruction.

The native test loads the **actual E32 DLL**, relocates it to `0x78000000`,
binds mocked Symbian OS imports and executes it on Unicorn's ARM926EJ-S CPU:

```sh
python3 -m venv .venv
.venv/bin/pip install unicorn==2.1.4
.venv/bin/python vincent/tests/test_e32_armv5_unicorn.py
```

Verified in RGB565, RGBA4444 and RGBA8888: 175 export addresses, UIDs/version,
zero E32 Data/BSS, clear/triangle/depth/lighting/fog/texture pixels, bitmap
presentation without a held heap lock, initial zero-sized window becoming
97×73, resize to 101×79 and deferred teardown. This mocks OS services: it does
not test real Window Server behavior, OS loader policy, device performance,
installation/signing, or full GLES/EGL conformance. The repository's existing
Gerbera tests also pass (4,422 checks).

## Port changes

- New EGL 1.1 and RAM surface adapter; per-thread configuration initialization
  replaces writable global constructor state.
- Portable no-JIT function-cache shim; stale Display/RasterizerPieces and
  unused custom arithmetic sources are excluded.
- GCC/Linux case/include fixes, 64-bit-host pointer tagging and public EABI
  symbol visibility; six absent GLES 1.1 ordinal wrappers added.
- Guard inactive varying indices for triangles, lines and points; validate
  active texture units before indexing state.
- Linear depth/stencil storage consistently used by clears and all primitive
  paths, with triangle edge-block bounds checks for arbitrary surface sizes.
  Initialize ALWAYS comparisons and suppress depth writes when depth testing
  is disabled.
- Libm-free exponential fog and bounded fixed-point specular power; immutable
  reciprocal table avoids a global constructor.
- Reuse the repository's shared freestanding headers, ARM division/soft-float
  helpers, native entry stub, import generator and zero-WSD linker script.
  **No PixelFlinger renderer or legacy JIT is linked into this DLL.**

## SDK use and deployment caution

The ZIP contains `sys/bin/libgles_cm.dll`, SDK import libraries under
`epoc32/release/armv5/lib/`, the frozen DEF, GLES headers, license and notices.
Link apps with `LIBRARY libgles_cm.lib` in their MMP using the supplied `.dso`
according to the SDK's normal EABI import-library convention. Native window
handles are `RWindow*`; the display can be `EGL_DEFAULT_DISPLAY`.

This DLL deliberately has the **same filename and UIDs** as the ROM GLES
implementation and the other two ports. These are mutually exclusive
alternatives, not libraries to install together. The ZIP is **not a signed
SIS installer**; actual replacement/loading requires the appropriate device
security policy and a recovery plan. Do not overwrite a working system driver
without a backup. ARMv5TE/EKA2 only; no emulator/WINSCW DLL is supplied.

Known limits include fixed-point internal precision and upstream feature gaps.
Float API entrypoints do not imply full floating-point state precision. Only
the scenarios above have regression coverage; multisampling, complete mipmap
and texture-combine behavior, all extensions, multithreading, and allocation
failure recovery are not exhaustively validated. No performance or conformance
claim is made based on upstream's older platform certifications.
