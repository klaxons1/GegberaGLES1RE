# AGENTS.md - working guide for this repository

You are working on a reverse engineering project: reconstructing clean, working
C from the decompilation of `libGLES_CM.dll` (OpenGL ES 1.1 CM driver for
Symbian, Nokia / Hybrid Graphics "Gerbera" stack). Read this file before doing
anything else, then read `PROGRESS.md` to see where the work stands.

## The ground truth

- `docs/libGLES_CM.dll` - the reference binary (x86 PE DLL, WINSCW/Symbian
  emulator build, GLES 1.1 + OES extensions, vendor string "Nokia", version
  "1.1.0").
- `docs/libGLES_CM.dll.c` - raw Hex-Rays decompilation (62k lines). This is
  INPUT, never edit it. Everything in it is derived from this file.
- `docs/libgles_cm.dll.epoc.c` - SECOND reference: the stripped ARM device
  build (44k lines, all sub_XXXX, no symbols, GNU C++). Use it to recover
  .data tables Hex-Rays truncated in the WINSCW decompile (e.g. the
  capability keys table) and to cross-check ambiguous code. Never edit.

## The workflow (caveman rules)

1. **Do not invent behavior.** Every function ported into `src/` must have a
   decompiled counterpart. Port from `generated/fragments/`, and when in doubt
   re-check the monolith. No "improvements" to algorithms, no renaming of
   concepts, no modernization that changes behavior.
2. **Keep assert sites verbatim.** The original code asserts with the ORIGINAL
   file name, line number and expression string, e.g.
   `__assert("hgInt32.inl", 294, "lo <= hi")` or
   `grAssert("expr", "oglError.inl", 34)`. Ported code keeps these exact
   strings (via `HG_ASSERT_AT(file, line, cond)` or direct calls). This keeps
   assert traces comparable against the reference binary and pins provenance.
3. **Names are sacred.** Function names (`hgClz32`, `F_ADD`,
   `IMatrix4x4_transpose`, ...), type prefixes (`HG`, `I`, `X`, `gr`, `grr`,
   `ogl`, `egl`, `hc`) and file names mirror the original tree. Keep them.
4. **Hex-Rays junk is cleaned, not preserved.** `__int8` -> `char`,
   `__ptr32` dropped, `__cv_XXXX` clone suffixes dropped, `MEMORY[0x...]`
   constructs are flagged in comments and resolved by reading the binary.
5. **Every ported module compiles and has tests** before it counts as done.
   The Makefile builds `src/` and runs `tests/`. `make test` must stay green.
6. **Hex-Rays lies sometimes.** Known lies: symbolic enum member names are
   swapped between translation units (e.g. `GR_PRIMITIVE_POINTS` displayed
   where the numeric value is a different enum's member - always check the
   numeric value against the binary when a constant looks wrong); stack
   variables can alias (`oldBMantissa[...]` in F_ADD is really named locals);
   inlined code can be attributed to the wrong function. When a decompiled
   fragment contradicts its own asserts or produces mathematically wrong
   results, trust the asserts and the math, and re-read the disassembly.

## Repository layout

```
docs/                  reference binary + decompilation (input, do not edit)
tools/split_decomp.py  splits the monolith, attributes functions to their
                       original files, dedupes TU clones, emits manifest
generated/             tool output (regenerable: python3 tools/split_decomp.py)
  fragments/<module>/  decompiled fragments grouped by original source file
  manifest.json        full function inventory (name, file, lines, clones)
  MANIFEST.md          readable summary
src/include/           headers (types, prototypes), mirroring original tree
src/<module>/          ported module sources
tests/                 unit tests (one per module: test_<module>.c)
Makefile               builds src/ into build/lib<module>.a + test binaries
```

## The module map

The original source tree (recovered from assert strings and PE strings inside
the binary) was `E:\3d_opengl_es_1_1\`:

| src/ module   | original tree                                                          | status |
|---------------|------------------------------------------------------------------------|--------|
| `src/hg`      | `renderer/libs/hg/` (hgDefs.h, hgInt32.inl, hgInt64Native.inl, hgFloat.c, hgFill.c, hgMemorySymbian.cpp) | DONE |
| `src/imath`   | `renderer/libs/imath/` (imApi, IScalar, IVector3/4, IMatrix3/4, INormal3, IUnitScalar, imInline) | in progress (scalar + vector/matrix done) |
| `src/gerbera` | `renderer/products/gerbera/core/implementation/` (gr*.inl, grSmall.c, rasterizer/grr*.inl) | DONE (core + grr) |
| `src/gerbera/api_opengl` | `renderer/products/gerbera/api_opengl/implementation/` (ogl*.inl, oglFast.c) | todo |
| `src/gerbera/hc` | `renderer/products/gerbera/hc/` (pixel-pipe JIT compiler, optimizer, regalloc, x86 & ARM32 backends, interpreter) | DONE |
| `src/egl`     | `src/` of the original project (eglApi.inl, eglBase.inl, eglUtil.inl)  | todo |
| `src/symbian` | `inc/symbian/eglSymbian.inl` + Symbian glue (grSymbian.cpp)            | todo |
| `src/platform`| CRT / C++ support / Symbian runtime pieces (reference only)            | n/a   |

`generated/MANIFEST.md` attributes every one of the 1402 unique functions in
the binary to one of these modules.

## Conventions in src/

- C99, compile with `-Wall -Wextra` and keep it warning-free.
- Types: use the hg typedefs (`HGint32`, `HGuint32`, `HGbool`, `HGfloat`,
  `HGint64`, ...) from `src/include/hg/hgDefs.h`.
- Asserts: `HG_ASSERT_AT("originalfile.inl", 123, cond)` for hg-style code;
  direct `grAssert("expr", "file", line)` / `imAssert(...)` calls for
  gerbera/imath code (implement those in their modules when ported, same
  semantics: evaluate expr, log file/line/expr, die).
- Function visibility: everything extern in the DLL is extern here; static
  helpers stay static in the module they belong to. TU-clone dedupe is already
  done by the splitter: port each unique function once, into its original
  file, and let the linker do the rest.
- Each module gets `tests/test_<module>.c` registered in the Makefile.

## Session protocol

1. `git status` + read `PROGRESS.md`.
2. Pick the next item from "Next up" in `PROGRESS.md` (or agree with the user).
3. Regenerate fragments if `docs/libGLES_CM.dll.c` changed:
   `python3 tools/split_decomp.py`.
4. Port module-by-module: header first, then sources, then tests, then
   `make test`. Do not leave the tree red.
5. Update `PROGRESS.md` (move items to DONE, refresh counts, note findings).
6. Commit small and often; describe what was ported and what was verified.

## Definition of done (per module)

- All unique functions of the module ported or explicitly marked not needed.
- Compiles with -Wall -Wextra, zero warnings.
- Unit tests cover the pure functions (math, bit tricks, format conversions);
  expected values computed independently, not by running the decompiled code.
- `PROGRESS.md` updated with the function checklist ticked.
