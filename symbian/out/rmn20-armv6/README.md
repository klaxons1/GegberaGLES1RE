# Gerbera GLES 1.1 for Symbian, ARMv6 build of rmn20's branch

This is a native Symbian OS v9.x (EKA2, S60 3rd Edition and later) build of
`libgles_cm.dll`. Its source is a different branch from the rest of this repo.

| | |
|---|---|
| Source | https://github.com/rmn20/GegberaGLES1RE, branch `arena/eae33090-gegberagles1re`, commit `123ffa4` ("Rebuild Win32 x86 libGLES_CM.dll") |
| Target | `-march=armv6j -mtune=arm1136jf-s` (ARM11, e.g. Nokia E72, N95, 5800). The JIT backend emits ARMv6 instructions (USAT, relaxed MUL/MLA), so this DLL does **not** run on ARMv5/ARM9 phones. |
| Framebuffer | Default 32-bit variant of that branch (not RGB565, no CCSOFT spans) |
| Toolchain | arm-none-symbianelf-gcc 14.2.0, elf2e32; branch's own `symbian/build_symbian.sh` |
| Size | `libgles_cm.dll` 235,004 bytes, 0 bytes of writable static data |
| SHA-256 | `8b4862c1957a6dc42bf7e6f2308c154223264991c285dbc4acac0051700730d5` (libgles_cm.dll) |

## Build fix applied

At `123ffa4` the branch does not build for Symbian.
`peepholeIdentityRepackStore` in `src/gerbera/hc/hcCompiler.c`, part of the
WIP identity-repack peephole, calls `calloc`, `malloc` and `free`, and the
freestanding Symbian runtime has none of them.

`symbian-malloc-fix.patch` changes the pass to take its scratch arrays from
the JIT compiler's own `HCArena`, the same `HCArena_mark` / `allocBottom` /
`HCArena_release` pattern the other compiler passes use. The pass logic is
unchanged.

## Checks

- The branch's host `make test` gives the same result with and without the
  patch. Both fail the same 9 checks at `tests/test_core.c:4347-4356`, which
  still expect the ARMv5 `MUL Rd != Rm` operand swap that the branch removed
  on purpose for ARMv6.
- The compiled DLL contains ARMv6-only instructions (366 of `uxth`, `uxtb`,
  `rev`, `usat`, etc.).
- Not yet tested on a phone.

## Speed (emulated CC3DBench, `symbian/emu`)

I compiled the same sources, at the same ARMv6 target, into the CC3DBench
emulator harness. It ran 73 scenes × 4 frames, and the table shows mean ARM
instructions per frame. Every frame CRC is identical to our JIT, so this build
renders bit-exactly the same images.

| Build | Instructions | vs our JIT |
|---|---|---|
| Our JIT (ARMv5, `symbian/out`, `symbian/out/rgb565`) | 794M | 1.00× |
| **This build** (rmn20 JIT, ARMv6) | 741M | **1.07×** |
| Our CCSOFT spans (ARMv5, `symbian/out/ccsoft`) | 608M | 1.30× |

Scene by scene, this build gains 10–16% on fill-bound scenes (blending,
overdraw, sprites, two textures) and 1–2% on geometry-bound ones.

## Contents

- `libgles_cm.dll`: goes to `sys/bin/` on the phone.
- `libgles_cm.dso`, `libgles_cm{000a0000}.dso`, `libgles_cm.def`: import
  library and def file for linking applications.
- `Gerbera-GLES1-Symbian-EKA2-ARMv6.zip`: the same files in an epoc32/sys
  layout, plus the GLES headers.
