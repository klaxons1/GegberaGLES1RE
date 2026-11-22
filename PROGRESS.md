# PROGRESS.md - live status of the reconstruction

Last updated: 2026-11-22 (session 2)

## Legend

- DONE = ported into `src/`, compiles warning-free, unit tested
- SCAFFOLDED = fragment files exist, nothing ported yet
- input = reference material only, not a port target

## Module status

| module | original tree | functions (unique) | status |
|---|---|---|---|
| hg | renderer/libs/hg | 105 | **DONE** (session 1) |
| imath | renderer/libs/imath | 198 | **IN PROGRESS** - scalar layer done (session 2) |
| gerbera/core | .../gerbera/core/implementation | 318 | SCAFFOLDED |
| gerbera/core/rasterizer (grr) | .../core/implementation/rasterizer | 26 | SCAFFOLDED |
| gerbera/api_opengl | .../gerbera/api_opengl/implementation | 313 | SCAFFOLDED |
| gerbera/hc | .../gerbera/hc | 296 | SCAFFOLDED |
| egl | src/ (eglApi.inl, eglBase.inl, eglUtil.inl) | 63 | SCAFFOLDED |
| egl_symbian | inc/symbian/eglSymbian.inl | 39 | SCAFFOLDED |
| crt/platform | Symbian + MSL runtime | 44 | n/a (reference) |

Total unique functions in the binary: 1402 (1776 definitions, of which 374 are
per-translation-unit inline clones that were deduped).

## DONE session 2 (imath scalar layer)

1. **Assert machinery ported** (`src/gerbera/grUtil.{c,h}`, `src/imath/imAssert.c`)
   - `grAssert(expr, file, line)`: prints `\nAssertion: %s (%s %d)\n`, returns 1
   - `grDie(v)`: fatal handler - reference crashes via null write; port aborts
   - `imAssert(expr, file, line)`: `grAssert(...)` then die (verbatim semantics)
2. **IScalar.c + imApi.h**: full scalar port - set/setAndNormalize, accessors,
   fixed-point conversions (fixedToScalar/scalarToFixed/toFixedHO/byteToScalar),
   arith on bit patterns via F_* helpers, compares/tests, degreesToRadians,
   log2_iterate + XScalar_log2, IUnitScalar.c (maxZero/lerp/tos), IMath.c
   validity checkers. All assert call sites keep the original file/line strings.
3. **hgFloat.c completed**: F_MUL/F_SQR/F_ITOF/F_RCP/F_RSQ + s_rcpTable[256] +
   s_rsqlut[384] (extracted verbatim from decompile lines 6780/7039).
4. **Tests**: test_imath.c added - 114 checks; total suite 402 checks, 0
   failures, warning-free. Library renamed `libgerbera.a`.

### Fidelity notes worth keeping (verified against decompile)

- `IScalar_iftos` NaN/Inf: keeps the sign BYTE, forces mantissa 0x7FFFFF
  (`a & 0xFF000000 | 0x7FFFFF`) - 0x7FC00000 -> 0x7F7FFFFF, not max-finite.
- `IScalar_byteToScalar`: the `m + 256` carry is original behavior;
  byte 128 -> 0x3F008081 (one ulp above b/255).
- `F_RCP` Newton chain: second refinement uses `HIWORD(scaled)` = `scaled >> 16`
  (got it wrong as the low word once - rcp(3.0) came out 0.5 instead of 1/3).
- `F_RSQ` is a LUT + polynomial estimate with ~1 ulp error; F_RSQ(4.0) returns
  0.49999995 (misses the 0.5 binade boundary) - faithful, do not "fix".
- `XScalar_log2(y)` returns `(log2(y) - 21) << 21` (y <= 0 -> 0x80000000);
  `IScalar_log2_iterate` is the 17-step shift-and-subtract chain with
  constants 1256197404 ... 23637 (constants = round(2^31*log2(1+2^-i))),
  final step (shift 17) does not update y.
- `F_MUL` truncates (1 ulp low sometimes): degreesToRadians(180) = 0x40490FDA.
- `IScalar_set` asserts m in (-0x40000000, 0x40000000) EXCLUSIVE (imApi.inl 169).

## DONE session 1

1. **Tooling** (`tools/split_decomp.py`)
   - parses the 62k-line monolith into 1776 function bodies
   - attributes every function to its original file using the assert strings
     (`grAssert(expr, "file", line)` leaks the original file + line), then
     name prefixes, then transitive call-site voting, then body types, then
     nearest-neighbour in decompilation order: 0 unattributed
   - dedupes `__cv_XXXX` per-TU clones (normalized body compare); 37 groups
     have divergent clones, all traced to Hex-Rays swapping symbolic enum
     names between TUs - listed in `generated/MANIFEST.md` for review
   - splits the data declarations per module as well
2. **hg module 100% ported** (`src/include/hg/` + `src/hg/`)
   - hgDefs.h (types + assert machinery), hgAssert.c
   - hgInt32.c: all 60+ int/64 helpers incl. saturating add, rotate,
     64-bit div (hgDiv32Internal/hgDivu64_32/hgDiv64_32), mulx64
   - hgFloat.c: NaN-safe bit-pattern compares (F_*), bit-level float adder
     (F_ADD/F_SUB), float builders, unit-scalar sin/cos (Taylor, fixed point)
   - hgFill.c: fill8/16/32 with the Duff-style unrolled core
   - hgMemory.h: thin wrappers over the C library
3. **Tests**: `tests/test_hg.c`, 288 checks, 0 failures, `-Wall -Wextra`
   clean.
4. **Docs**: AGENTS.md, README.md, docs/analysis.md, this file.

## Next up (recommended order)

1. **imath (remaining)** - vectors/matrices next: IVector3/IVector4 (isValid
   bodies already located; struct `_IVector3{int e; int m[3]}`), INormal3,
   IMatrix3/4 (`_IMatrix4x4` = 4 rows of `{ HGuint32 e; HGint32 m[4]; }`),
   then imInline.inl types (XScalar ops, XColor3, IFVector3/4 - needed by
   attenuateRcp/Rsq which stay deferred until then), imApi.inl remainder,
   IMath.c vector isValid functions move in with the vector port.
2. **gerbera/core** - biggest module (15.3k decompiled lines). Needs the
   `Gerbera_s` context struct reconstructed first; the gr*.inl files hang off
   it. Port order: grUtil/grRGBA -> grDefs tables -> grrClear -> grrBlit ->
   grrTexture -> grScanConvert -> grRasterizer -> grFill -> grClipper ->
   grTessellator -> grVertexPipeline -> grInterpreter -> grPixelPipeline ->
   grPipelineCache -> grApiFast/grApiSmall -> grSmall.c.
3. **gerbera/hc** - the vertex-processor JIT compiler (9.4k lines). Self
   contained; needs HCInstr_s/BasicBlock_s/HCArena_s/etc. struct layouts.
   Opcodes live in hcOpcode.h with names visible in the decompilation
   (`HCOP_MOV`, `HCOP_LABEL`, `HCOP_EOP`, ...).
4. **api_opengl** - GL entry points + getters + texture/buffer-object
   managers. Needs oglContext_s. This is where the 158 `gl*()` exports land.
5. **egl** + **egl_symbian** - EGL 1.x API + Symbian window/pixmap glue.
   The Symbian classes (RWindow, CFbsBitmap, ...) need a portability shim
   (src/platform) - decide: stub them or write a hosted backend.
6. Final: link everything into a libGLES_CM replacement, diff-test against
   the reference binary on x86.

## Known unknowns / findings to remember

- Missing `defs.h`: all struct/enum layouts lived in Hex-Rays local types.
  They must be reconstructed from field accesses in the fragments.
- Enum VALUES are mostly unknown; member NAMES are recoverable from the
  fragments (`_enum_36hcCompiler_c__HCOP_EOP` etc.) and GL constants from
  numeric comparisons in the code (e.g. 7680 = GL_KEEP, 5888 = GL_INVERT).
- `glp*`/`off_470224` are the GL dispatch table entries
  (`procedures` string table starts at "glWeightPointerOES").
- The binary imports CONE.dll, WS32.dll, FBSCLI.dll, GDI.dll, euser.dll,
  hal.dll - Symbian emulator dependencies (window server, font bitmap server).
- `grDie` is the driver's fatal handler (`grDie(MEMORY[0])` in decompiled
  asserts); implement as abort + message when porting gerbera core.
- Assert call conventions: hg uses `__assert(file, line, expr)`; gerbera core
  uses `grAssert(expr, file, line)` + `grDie`; imath uses
  `imAssert(expr, file, line)`. Keep them distinct when porting.
- The x86 JIT (hcX86Backend.c) emits native code for the emulator build; its
  "emulator" (hcEmulator.c) interprets the same opcodes. Port both.
