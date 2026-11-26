# Gerbera GLES 1.1 for Symbian: ARMv6 rmn20 build with an optimised JIT raster loop

This is a native Symbian OS v9.x (EKA2) build of `libgles_cm.dll` from rmn20's
branch. It adds new optimisation passes to the Gerbera JIT that compiles the
per-pixel raster loops. `../rmn20-armv6/` holds the build of the same branch
without these passes.

| | |
|---|---|
| Source | https://github.com/rmn20/GegberaGLES1RE, branch `arena/eae33090-gegberagles1re`, commit `123ffa4`, plus `rmn20-jitopt.patch` |
| Target | `-march=armv6j -mtune=arm1136jf-s` (ARM11). ARMv6 only, like `../rmn20-armv6/` |
| Toolchain | arm-none-symbianelf-gcc 14.2.0, elf2e32; branch's own `symbian/build_symbian.sh` |
| Size | `libgles_cm.dll` 251,168 bytes (+16 KB for the new passes), 0 bytes of writable static data |
| SHA-256 | `71493c00827606e57181da5c13743469aa9315d738fec3616ec0cc86ab496ce7` (libgles_cm.dll) |

`rmn20-jitopt.patch` applies with `git apply` to a clean checkout of `123ffa4`.
It also contains `../rmn20-armv6/symbian-malloc-fix.patch`, which the branch
needs before it builds for Symbian.

## What changed in the JIT

These are new passes. The JIT front-end that generates the IR is unchanged.

**IR level (`src/gerbera/hc/hcSchedule.c`)**

- `HC_pressureSchedule` runs before register allocation. It is a bottom-up
  list scheduler that reorders each block to keep as few values live as
  possible, so the register allocator spills less.
- `HC_localCSE`: common subexpression elimination inside a block. It removes
  repeated computations of the same value that the generator emits.

**Machine code level (`src/gerbera/hc/hcARMSched.c`)**

This pass runs after ARM code generation and before branch patching.

- ARM11 peepholes:
  - `LSR`+`AND #255` becomes `UXTB …, ROR #n`.
  - `LSR #8`+`USAT` becomes `USAT …, ASR #8`, only where the backend proved the
    value is non-negative.
  - `LSR #n`+`LSL #n` becomes `BIC`.
  - `MUL`+`ADD` becomes `MLA` (the fog blend).
  - `CMP #0` before a branch becomes the S bit on the instruction that set the
    register (the loop counter).
- Redundant frame-load forwarding: when one frame slot is loaded twice in a
  block, the second load is removed and both uses share a free register.
- Register renaming: after allocation, short temporaries move to registers that
  are free, so reused registers no longer block reordering.
- List scheduler using ARM1136 latencies: 3-cycle loads, 2–3-cycle
  multiplies, the late/early register-read rules, and a single issue slot. It
  fills load-use and multiply-use stalls. The loop branch always stays last.

All passes are on by default. For A/B measurements on host builds, the global
`HC_sched_disable` turns them off one by one: 1 pressure schedule, 2 CSE,
4 peepholes, 8 scheduler, 16 USAT hints, 32 renaming, 64 load forwarding. On
Symbian it is the constant 0. To compare, run `tests/arm_jit_bench.py --disable N`.

## Results

### Raster loop (`tests/arm_jit_bench.py`)

The bench compiles each pipeline with the JIT and runs the ARM code under
unicorn, with an ARM1136 cycle model (issue, result latencies, interlocks).
It checks every output pixel against a C reference. The figures are the
marginal cost per pixel. The cycle figures come from this model, not from a
phone.

| Configuration | before: ins / cyc per px | after: ins / cyc per px | speed-up (cycles) |
|---|---|---|---|
| texture, no vertex colour (`bench_tex_nearest_rgba_no_vcolor`) | 9 / 16 | 9 / 13 | 1.23× |
| texture + vertex colour (`bench_tex_nearest_rgba_vcolor`) | 59 / 113 | 41 / 61 | **1.85×** |
| texture + vertex colour + fog (`…_vcolor_fog`) | 78 / 156 | 56 / 74 | **2.11×** |
| texture RGB565 replace | 11 / 19 | 11 / 15 | 1.27× |
| decal | 63 / 108 | 46 / 61 | 1.77× |
| blend src-alpha / 1-src-alpha | 56 / 106 | 47 / 61 | 1.74× |
| modulate + blend | 94 / 185 | 80 / 109 | 1.70× |
| two textures, modulate | 96 / 185 | 69 / 109 | 1.70× |
| flat / gouraud | 17 / 20 | 14 / 15 | 1.33× |
| gouraud + depth test | 22.4 / 35.4 | 18.0 / 22.8 | 1.55× |
| texture + vertex colour + depth | 48.3 / 85.5 | 33.4 / 45.3 | 1.89× |
| texture + vertex colour + alpha test | 58.7 / 112.6 | 43.7 / 56.3 | 2.00× |
| alpha test | 19.7 / 24.8 | 19.7 / 20.8 | 1.19× |

The "before" column is this same DLL's code generator with
`HC_sched_disable = 127`. It reproduces the unpatched branch exactly.

### Whole frames (emulated CC3DBench, `symbian/emu`)

The emulator ran 73 scenes × 4 frames at the same ARMv6 target. **Every frame
CRC is identical** to the unpatched rmn20 build.

| Build | ARM instructions, whole run |
|---|---|
| rmn20 ARMv6 (`../rmn20-armv6`) | 741M |
| **this build** | **691M (1.07×)** |

The gains reach 1.15–1.23× on fill-bound scenes: textures, sprites, particles,
additive and premultiplied blending, two textures. Lighting-bound scenes gain
0–3%. The emulator counts instructions only. Most of the raster-loop gain comes
from removing pipeline stalls, which the scheduler does without removing
instructions, so these whole-frame figures understate the speed-up on a real
ARM11.

## Checks

- `tests/differential.py --arch arm --module raster --no-coverage`:
  5500 comparisons, 0 mismatches.
- `tests/arm_jit_harness.py`: 15/15. `tests/arm_jit_bench.py`: 18/18 bit-exact.
- `make test`: the same 9 checks fail as on the unpatched branch
  (`tests/test_core.c:4347-4356`, which expect the ARMv5 MUL operand swap).
- Emulated CC3DBench: 73/73 scenes with identical CRCs.
- Not yet tested on a phone.

## Not done yet (possible next steps)

- The fog loop still keeps the fog value in a stack slot (`[r0,#0x17c]`) and
  reloads 7 per-pixel deltas each pixel, because the loop needs more registers
  than ARM has. A spill cost that treats invariant frame loads as cheap to
  recompute would leave registers for these values.
- SWAR colour maths:
  - modulate R and B in one register with an `0x00FF00FF` mask, or use
    `UXTB16`;
  - drop the `USAT` after modulate where the result is provably at most 255.

  Both need changes in the front-end code generator, not only in these passes.

## Contents

- `libgles_cm.dll`: goes to `sys/bin/` on the phone.
- `libgles_cm.dso`, `libgles_cm{000a0000}.dso`, `libgles_cm.def`: import library
  and def file for linking.
- `Gerbera-GLES1-Symbian-EKA2-ARMv6.zip`: the same files in an epoc32/sys layout,
  plus the GLES headers.
- `rmn20-jitopt.patch`: the source changes, including the bench
  (`tests/arm_jit_bench.py`) and the adapter helpers it uses.
