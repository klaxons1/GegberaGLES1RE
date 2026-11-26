# Gerbera GLES 1.1 for Symbian: ARMv6 rmn20 build, JIT raster loop optimisation round 2

This is a native Symbian OS v9.x (EKA2) build of `libgles_cm.dll` from rmn20's
branch. It contains everything in `../rmn20-armv6-jitopt/` plus a second round
of JIT optimisations:

- loop rotation;
- RGB565-specific peepholes;
- a tighter value-range rule.

`../rmn20-armv6/` is the same branch without any of these passes.

| | |
|---|---|
| Source | https://github.com/rmn20/GegberaGLES1RE, branch `arena/eae33090-gegberagles1re`, commit `123ffa4`, plus `rmn20-jitopt2.patch` |
| Target | `-march=armv6j -mtune=arm1136jf-s` (ARM11). ARMv6 only |
| Toolchain | arm-none-symbianelf-gcc 14.2.0, elf2e32; the branch's own `symbian/build_symbian.sh` |
| Size | `libgles_cm.dll` 259,048 bytes (+7.9 KB over jitopt), 0 bytes of writable static data |
| SHA-256 | `de0317bc48ab40073c91eeda0daf5d123718e78f87f29d0e254cfe78a8a0f7cb` (libgles_cm.dll) |

`rmn20-jitopt2.patch` applies with `git apply` to a clean checkout of `123ffa4`.
It contains all the source changes, including the first round. It replaces
`../rmn20-armv6-jitopt/rmn20-jitopt.patch` and does not go on top of it.

## What is new in this round

All changes are in the machine-code pass (`src/gerbera/hc/hcARMSched.c`) and one
range rule in the ARM back end. The front end that generates the JIT IR is
unchanged.

### Loop rotation and variant selection

**Rotation.** In a self-loop, the first instructions of the body usually wait
for loads: the texel address, the texel, then the depth or destination pixel.
Rotation copies the start of the next iteration into the end of the current
one, so those loads issue before the back-branch:

- loads, and anything that may fault, are copied as conditional (`ldrne`);
- plain ALU copies may run unconditionally when that is safe.

The scheduler can then hide their latency behind the colour maths.

**Variant selection.** Rotation helps some loops and hurts others. The pass
therefore builds 6 variants of each loop: no rotation, conditional-only copies
or unconditional ALU copies, each with or without a loop-carried scheduling
bonus. It estimates each one with an ARM1136 timing model run over 4
iterations and keeps the cheapest.

### RGB565 peepholes

- **Channel expand fold.** RGB565 → RGB888 expansion of one channel used to take
  4 instructions: `(s >> p) & mask`, then `x | x << n`, then `>> q`. It now
  takes 3: `AND t, s, #(mask << p)` followed by two shifts. There are two forms
  of the rewrite, so it also works when every step reuses the same register,
  which is how the destination pixel is read in blending.
- **USAT/LSR fold.** For the 8-bit to 5/6-bit repack before storing a 565
  pixel, `USAT d, #8, x, ASR #8` followed by `d, LSR #3` becomes
  `USAT d, #5, x, ASR #11`, because `usat(y, n) >> k == usat(y >> k, n - k)`.
  When the user was a `MOV`, the `USAT` writes the target register directly.
- **Range rule.** In `armRangeMax` (`hcARMBackend.c`), `LSR #imm` now narrows
  the known maximum to `max(src) >> imm`. That proves more `USAT`s safe to fuse
  with their shift.

`HC_sched_disable` bit `128` turns rotation off for host A/B runs. Bit `4`
already turned off the peepholes. On Symbian, `HC_sched_disable` is a
compile-time 0.

## Results

### Raster loop (`tests/arm_jit_bench.py`, ARM1136 cycle model, cycles per pixel)

The bench runs the generated ARM code under unicorn with an ARM1136 timing
model, and checks every pixel against the C reference. These figures come from
that model, not from a phone.

- **Off:** every pass disabled (`--disable 255`). This equals the unpatched
  branch.
- **jitopt:** `../rmn20-armv6-jitopt/`.
- **jitopt2:** this build.

| Configuration | off | jitopt | **jitopt2** | vs off |
|---|---|---|---|---|
| **RGB565 target + RGB565 texture, modulate** (the CC3D game scenes' loop) | 98 | 64 | **51** | 1.92× |
| … + depth test | 121 | 79 | **69** | 1.75× |
| RGB565 target + RGB565 texture, replace + depth | 67 | 51 | **48** | 1.40× |
| RGB565 target + RGB565 texture, modulate + fog | 136 | 78 | **66** | 2.06× |
| RGB565 target + RGBA texture, modulate + alpha blend | 186 | 105 | **96** | 1.94× |
| RGBA texture + vertex colour | 113 | 61 | **57** | 1.98× |
| RGBA texture + vertex colour + fog | 156 | 74 | **72** | 2.17× |
| decal | 108 | 61 | **57** | 1.89× |
| blend src-alpha / 1-src-alpha | 106 | 61 | **58** | 1.83× |
| two textures, modulate | 185 | 109 | **106** | 1.75× |
| modulate + blend (RGBA target) | 185 | 109 | 109 | 1.70× |
| texture replace, no vertex colour | 16 | 13 | 13 | 1.23× |
| texture + vertex colour + depth (RGBA) | 85.5 | 45.3 | 45.3 | 1.89× |
| flat / gouraud | 20 | 15 | 15 | 1.33× |

The cases that were already well scheduled in jitopt (depth, flat, replace)
are unchanged.

### Whole frames (emulated CC3DBench, `symbian/emu`)

The emulator ran 73 scenes × 4 frames, ARMv6 target, 16-bit surface. **Every
frame CRC is identical** to the unpatched rmn20 build.

| Build | ARM instructions, whole run |
|---|---|
| rmn20 ARMv6 (`../rmn20-armv6`) | 741M |
| jitopt (`../rmn20-armv6-jitopt`) | 691M (1.07×) |
| **jitopt2 (this build)** | **675M (1.10×)** |

Fill-bound scenes gain 1.20–1.33×. Examples: cutout, env add, the z-buffer
scenes, tex 16/256, particles, additive and premultiplied blending.

The emulator counts instructions, not cycles. Much of the gain comes from
removing stalls, which this count does not show.

## Checks

- **`tests/arm_jit_fuzz.py` (new):** 4,600 random pipelines. They cover:
  - an RGBA8 or RGB565 target;
  - 0–2 texture units, 8 texture formats, 5 env modes, nearest or bilinear
    filtering;
  - depth, alpha test, blend factors, colour and monochrome fog;
  - random gradients and span lengths.

  Each one is compiled with all passes on and with all passes off. The colour,
  depth and stencil buffers both write are identical in every case.
- `tests/arm_jit_bench.py`: 23/23 bit-exact against the C reference.
  `tests/arm_jit_harness.py`: 15/15.
- `tests/differential.py --arch arm --module raster --no-coverage`: 5,500
  comparisons, 0 mismatches.
- `make test`: the same 9 checks fail as on the unpatched branch
  (`tests/test_core.c:4347-4356`, which expect the ARMv5 MUL operand swap).
- Emulated CC3DBench: 73/73 scenes with identical CRCs.
- Not yet tested on a phone.

## Still left on the table

- The loops are register-bound. Every pixel reloads loop-invariant frame
  fields: texture shift, texel pointer, colour-buffer base and the 4–6
  gradient deltas. That is about 10 of 40–55 instructions. Keeping them in
  registers needs a front-end change, for example walking a colour-buffer
  pointer instead of base + index, so registers are free for them.
- `estimateLoop` sometimes picks a variant 1–2 cycles slower than the best one:
  vcolor 57 against a possible 55, fog 72 against 71.

## Contents

- `libgles_cm.dll`: goes to `sys/bin/` on the phone.
- `libgles_cm.dso`, `libgles_cm{000a0000}.dso`, `libgles_cm.def`: import library
  and def file.
- `Gerbera-GLES1-Symbian-EKA2-ARMv6.zip`: the same files in an epoc32/sys layout,
  plus the GLES headers.
- `rmn20-jitopt2.patch`: all source changes against `123ffa4`, including:
  - the bench `tests/arm_jit_bench.py`, with the new `y565_*` cases;
  - the fuzzer `tests/arm_jit_fuzz.py`;
  - the adapter helpers they use.
