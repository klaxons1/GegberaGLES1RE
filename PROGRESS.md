# PROGRESS.md - live status of the reconstruction

Last updated: 2026-11-22 (session 1)

## Legend

- DONE = ported into `src/`, compiles warning-free, unit tested
- SCAFFOLDED = fragment files exist, nothing ported yet
- input = reference material only, not a port target

## Module status

| module | original tree | functions (unique) | status |
|---|---|---|---|
| hg | renderer/libs/hg | 105 | **DONE** (this session) |
| imath | renderer/libs/imath | 198 | SCAFFOLDED |
| gerbera/core | .../gerbera/core/implementation | 318 | SCAFFOLDED |
| gerbera/core/rasterizer (grr) | .../core/implementation/rasterizer | 26 | SCAFFOLDED |
| gerbera/api_opengl | .../gerbera/api_opengl/implementation | 313 | SCAFFOLDED |
| gerbera/hc | .../gerbera/hc | 296 | SCAFFOLDED |
| egl | src/ (eglApi.inl, eglBase.inl, eglUtil.inl) | 63 | SCAFFOLDED |
| egl_symbian | inc/symbian/eglSymbian.inl | 39 | SCAFFOLDED |
| crt/platform | Symbian + MSL runtime | 44 | n/a (reference) |

Total unique functions in the binary: 1402 (1776 definitions, of which 374 are
per-translation-unit inline clones that were deduped).

## DONE this session

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

1. **imath** - self-contained fixed-point math. Start with IScalar.c +
   IUnitScalar.c (IUnitScalar_sin/cos already ported into hgFloat.c as
   provenance showed hgFloat.c), then imInline.inl types (XScalar, XColor3,
   IFVector4...), then IVector3/4, INormal3, IMatrix3/4, IMath.c, imApi.inl.
   Needs: imApi.h type reconstruction - `_IMatrix4x4` is a struct of 4 rows
   `{ HGuint32 e; HGint32 m[4]; }` (per-row packed exponents + mantissas),
   verify per-field from the fragment bodies.
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
