# analysis.md - reverse engineering notes on libGLES_CM.dll

## The binary

- `libGLES_CM.dll`, 1.5 MB, PE32 x86 DLL (Symbian WINSCW emulator build).
- PE export directory: single table of 178 exports (see below).
- Imports: CONE.dll, ESTLIB.dll, FBSCLI.dll, GDI.dll, WS32.dll, euser.dll,
  hal.dll, kernel32.dll, user32.dll (Symbian emulator + Win32).
- Compiled with Metrowerks CodeWarrior for Symbian (MSL runtime traces:
  `__msl_*`, `ThreadLocalData.cp`, `NMWExceptionX86.cpp`).
- Embedded strings identify the product:
  - `E:\3d_opengl_es_1_1\...` original source tree
  - `renderer\products\gerbera\...` the internal product name "Gerbera"
  - `eglSymbian.inl`, `hgDefs.h`, `imApi.h`, `hcX86Backend.c`, ...
- GL identity (from `g_getStrings`): vendor "Nokia", version "1.1.0",
  renderer "OpenGL ES-CM 1.1", extensions:
  `GL_OES_byte_coordinates GL_OES_fixed_point GL_OES_single_precision
   GL_OES_read_format GL_OES_query_matrix GL_OES_compressed_paletted_texture
   GL_OES_matrix_palette GL_OES_point_sprite GL_OES_point_size_array
   GL_OES_draw_texture GL_OES_matrix_get GL_OES_swap_control
   GL_OES_render_texture`

## The original source tree

Recovered from assert strings (file names) and PE strings (full paths):

```
E:\3d_opengl_es_1_1\
├── src\                                  EGL layer
│   ├── eglApi.inl                        EGL entry points
│   ├── eglBase.inl                       display/context/surface base
│   └── eglUtil.inl                       configs, refcounts, blit
├── inc\symbian\
│   └── eglSymbian.inl                    native window/pixmap glue
├── renderer\libs\hg\                     Hybrid Graphics support lib
│   ├── hgDefs.h  hgInt32.inl  hgInt64Native.inl
│   ├── hgFloat.c/.h/.inl  hgFill.c  hgMemorySymbian.cpp
├── renderer\libs\imath\                  fixed point math library
│   ├── interface\imApi.h/.inl
│   └── implementation\IMath.c IScalar.c IUnitScalar.c IVector3.c IVector4.c
│       IMatrix3.c IMatrix4.c INormal3.c imInline.inl
└── renderer\products\gerbera\            the driver itself
    ├── api_opengl\implementation\
    │   ├── oglApi.inl oglContext.inl oglGetter.inl oglUtil.h/.inl
    │   ├── oglError.inl oglDescriptor.inl oglTexManager.inl
    │   ├── oglBufObjManager.inl oglMatrixStack.inl oglFast.c
    ├── core\implementation\
    │   ├── grDefs.h grUtil.h grUtilSmall.inl grRGBA.h
    │   ├── grApiFast.inl grApiSmall.inl grSmall.c
    │   ├── grScanConvert.inl grRasterizer.inl grTessellator.inl
    │   ├── grFill.inl grClipper.inl grVertexPipeline.inl
    │   ├── grInterpreter.inl grPixelPipeline.h/.inl grPipelineCache.inl
    │   ├── rasterizer\grrPrimitives.inl grrClear.inl grrTexture.inl grrBlit.inl
    │   └── symbian\grSymbian.cpp
    └── hc\                               vertex processor JIT compiler
        ├── interface\hcOpcode.h
        └── implementation\hcBitSet.h/.c hcAdjMtx.h/.c hcAdjList.c
            hcDUChain.h/.c hcBackend.h hcMemory.h/.c hcBucketDefs.h/.c
            hcBasicBlock.h hcQueue.h/.c hcStack.h/.c hcCompiler.c
            hcPanicRegalloc.inl hcDefs.c hcEmulator.c hcOpcode.c
            hcRandom.c hcX86Backend.c hcPrivateDefs.h
```

## Function inventory

`tools/split_decomp.py` parses `docs/libGLES_CM.dll.c` (62k lines):

- 1776 function definitions parsed
- 1402 unique functions after deduping the 374 per-TU `__cv_XXXX` inline
  clones
- attribution (how each function was mapped to its original file):
  assert strings > name prefix > transitive call-site voting > body types >
  nearest neighbour in decompilation order. Final: 0 unattributed.

| module | unique funcs | decomp lines | notes |
|---|---|---|---|
| api_opengl | 313 | 8674 | includes all 158 `gl*()` entries |
| core | 318 | 15341 | gr* rasterizer + interpreter |
| hc | 296 | 9358 | JIT compiler + x86 backend |
| imath | 198 | 5529 | fixed point math |
| hg | 105 | 1055 | support lib |
| egl | 63 | 2489 | EGL 1.x |
| egl_symbian | 39 | 1042 | native glue |
| grr | 26 | 1913 | rasterizer inner loops |
| crt | 44 | 1303 | runtime, reference only |

37 clone groups have divergent bodies; all inspected cases are Hex-Rays
displaying different symbolic names for the same numeric enum value in
different TUs (e.g. `GR_PRIMITIVE_POINTS` vs `GR_MATRIXMODE_MODELVIEW` for 0).
They are listed in `generated/MANIFEST.md`.

## Assert conventions (per layer)

| layer | assert call | order |
|---|---|---|
| hg | `__assert(file, line, expr)` | file, line, expr |
| gerbera core | `grAssert(expr, file, line)` then `grDie(...)` | expr, file, line |
| imath | `imAssert(expr, file, line)` then dies | expr, file, line |

The assert strings are the primary evidence for function->file attribution
and preserve the original line numbers of the code.

## Key data recovered

- `g_colorFormats[4]` - {2bpp 4444}, {2bpp 565}, {4bpp 888}, {4bpp 8888}
- `g_depthFormats[3]`, `g_stencilFormats[2]`, `g_surfaceTypes[3]`
- `g_primitiveTypes[8]` - maps GL prim enums to internal prim classes
- `g_getStrings[4]` - vendor/version/renderer/extensions (above)
- `s_defaultValues` - the full initial GL state blob (~500 entries incl.
  matrices, viewport, fog, lights, clip planes, texture env)
- getter token tables (`s_getterTokens`, `keys`, `values`) drive
  `glGetIntegerv`/`glGetFloatv` dispatch
- GL dispatch: `procedures` string table + `off_470224` etc. function
  pointer table (starts at "glWeightPointerOES")

## Hex-Rays artifacts to watch for

1. `__cv_HEX` clones - one copy per including TU; identical after normalizing
   enum type ids and symbolic names. The splitter dedupes them.
2. Symbolic constant confusion - enum member display names come from the
   first enum with that value in the TU; verify numerics against GL headers.
3. `LODWORD/HIDWORD` - 64 bit values split for x86; in clean code these are
   plain 64-bit operations (hg was originally written with native 64-bit:
   hgInt64Native.inl).
4. `MEMORY[0x...]` - unresolved memory refs; usually assert-path noise
   (`grDie(MEMORY[0])`) or TLS; resolve by reading the disassembly.
5. Stack variable aliasing - decompiler sometimes shares one stack slot
   across logically distinct locals (see F_ADD); rewrite with named locals.
6. `// weak`, `// idb` markers on data - values confirmed stable anyway.
7. Inline asm in 3 places (jmp tables, cpuid in `__detect_cpu_instruction_set`)
   - runtime pieces, not part of the driver port.

## Export surface

178 exports; the GL 1.1 entry points (glBlendFunc..glWeightPointerOES etc.),
EGL 1.0 entry points, plus driver-private exports. The export table itself is
in the binary at RVA 0x94000; GL names also appear in the `procedures` string
table used for `eglGetProcAddress`.

## The ARM build (`libgles_cm.dll.epoc` + `.epoc.c`, `libgles_cm.dso`)

- `libgles_cm.dll.epoc` (202 KB) - Symbian **EKA2 target image**, ARM code
  (header words `0x10000079 ... "EPOC"`); produced by **GNU C++ / GCCE**
  (the decompiler banner says `Detected compiler: GNU C++`, unlike the
  Metrowerks x86 build). Decompilation: `docs/libgles_cm.dll.epoc.c`
  (44.5k lines, `nfuncs=953 queued=866 decompiled=866`, all functions
  unnamed `sub_XXXX`, **no assert strings** - release build compiled them out).
- `libgles_cm.dso` (8 KB) - ELF32 **ARM** static import library (no code).
- VA-to-file mapping in the image: `file_off = VA - 0x7F64` (verified against
  the backend vtable, code at VA 0x8028+, data at VA 0x36xxx).

### ARM hc backend (recovered session 4c)

The x86 source tree (assert strings) only ever mentions `hcX86Backend.c`; the
ARM build ships its **own JIT backend** (unnamed in the decomp - likely
`hcARMBackend.c` in the original tree). Identification path:
`grEnsureValidPixelPipeline` (sub_20904) -> `grGetBackend` (sub_25484 ->
`off_36F6C`) -> `HC_compile` (sub_2C76C/sub_2C6E8/sub_2C298 = compileWrap).

The decompiler typed the vtable as `[2]` entries; the full 12-slot
`HCBackend_s` was recovered from the binary at **file offset 0x2F008**
(VA 0x36F6C), immediately followed by the `swap[] = "3412/0-.+,)*"` string -
same data order as x86 `backendDecl`:

| slot | member | ARM target | ARM value / role | x86 (hcX86Backend.c) |
|---|---|---|---|---|
| +0 | canCreateImmediate | sub_245C0 | ARM rotated-imm8 encodability check on the instruction immediate | returns 1 |
| +4 | internalStackSpace | sub_24100 | **60** | **40** |
| +8 | internalStackSpaceAfterInvars | NULL | - | NULL |
| +12 | targetRegisterMapping | sub_23D7C | `&word_36F2A` = bytes `{0..12,14}` (r0-r12,r14; r13/sp skipped) | `&regRemap` |
| +16 | numHardRegs | sub_24108 | **14** | **8** |
| +20 | preRegallocFixer | sub_2417C | 4-arg (mgr,dest,src,handle) | present |
| +24 | codegen | sub_24AEC | emits ARM words (`cond<<28 \| 0x08000000 ...`, `0xE0000000` AL), ~410 decomp lines | HCX86Backend_codegen |
| +28 | postRegallocFixer | NULL | compileWrap falls back to sub_25E34 | present |
| +32 | emulate | sub_24118 | `return program(stackPtr);` | same |
| +36 | (slot 9, unnamed) | sub_25480 | 4-byte `bx lr` no-op (decomp skipped it) | NULL |
| +40 | isNative | sub_24110 | **1** | 1 |
| +44 | optimizationMask | NULL | compileWrap checks NULL before `&1` | NULL |

ARM codegen emits 32-bit ARM instruction words via helpers sub_23F2C (emit
word + track used-reg mask), sub_24090 (data-processing field assembly:
cond in bits 31-28, I/opc/S fields), sub_24674/24734/2488C/248F4/2496C
(load/store + operand decoding), sub_24ABC (128-byte code-buffer growth).
`HC_opcodeDecls`-driven switch inside sub_24AEC (cases 4,5,6,7,8,0xB).

The ARM `grExecuteProgram` (sub_1F87C, bytecode VM) matches the x86
semantics line-for-line (verified against the session-4b port): big-endian
16-bit offsets, HC-instruction inner loop without pop, repeat-frame
`[limit, counter]` with cases 0xE/F/0x10 reading the counter.

## Strategy for full reconstruction

1. Port leaf libraries first (hg done, imath next) - no struct unknowns.
2. Reconstruct struct layouts for gerbera/hc/api/egl from field accesses in
   fragments (field names are preserved by the decompiler); cross-check
   offsets against the binary where the code uses raw offsets.
3. Port per-module with unit tests; the rasterizer (grr*) can be validated
   by rendering and diffing against a software reference.
4. Keep assert strings verbatim so a future dynamic run of the rebuilt DLL
   can be diffed against the original trace-by-trace.
