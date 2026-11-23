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

ARM codegen emits 32-bit ARM instruction words via helpers sub_23BF8
(word/byte/halfword/signed load-store), sub_23F04 (`B<cond>` branch), sub_23F2C
(data-processing + barrel-shifter + rotated-imm8), sub_24090 (`STMFD`/`LDMFD`
block transfer), sub_24674/24734/2488C/248F4/2496C (memop/minmax/ALU/shifted-ALU
/S-bit-fused cond-branch), and sub_24ABC (128-byte code-buffer growth), driven
by the 55-entry `armOpcodeInfo` table at VA `0x36EBC` (cases 1..8, 11, 12).
Ported in full to `src/gerbera/hc/hcARMBackend.c` (session 4e) after verifying
all 7 static tables (`VA 0x36EB8..0x36F69`) and disassembling 3 IDA
register-loss sites (`0x23D14` `+0x02000000` LDR/STR I-bit, `0x23DA8`/`0x251E4`
`excludeMask` in `r2` for `MUL rd,rd,rd`, and `0x2406C` `+ rm` in shifted DP).

The ARM `grExecuteProgram` (sub_1F87C, bytecode VM) matches the x86
semantics line-for-line (verified against the session-4b port): big-endian
16-bit offsets, HC-instruction inner loop without pop, repeat-frame
`[limit, counter]` with cases 0xE/F/0x10 reading the counter.

### CodeView NB11 `sstGlobalTypes` & `hc` binary quirks (recovered session 4f)

- `docs/libGLES_CM.dll` embeds a complete CodeView `NB11` debug directory at
  file offset `0x98000` (`sstGlobalTypes` at `0x142F58`, `3,656` type records),
  preserving exact struct field names, byte offsets, and bitfield widths even
  for anonymous typedef structs (`HCBitSet_s`, `HCAdjMtx_s`, `HCAdjList_s`,
  `DUChain_s`, `UDChainStub_s`, `HCBucketDefs_s`, `BasicBlock_s`, `HCQueue_s`
  `TI=0x1A9B`, `HCStack_s` `TI=0x1AA0`, `AdjEntry_s` `TI=0x1AAB`
  `{spillCost:31, spill:1, color:6, disp:26}`, `LREntry_s`, `CopySet_s`
  `{dst:16, src:16, shift:6, type:2, pad:24}`, peephole rule `TI=0x1A7D`,
  `FillBlock_s`, and `CompParams_s` `TI=0x1B70`).
- Two original Gerbera C quirks verified directly in machine code:
  1. `emulator_substitute` (`hcEmulator.c`): in the 2-register conditional
     branch loop (`VA 0x45DD84..0x45DD9E` on x86; `sub_2CABC` on ARM), the
     original C code shadowed outer `op` (`int op = ops[i] & 0xff; if (op == op)`),
     compiling literally to `cmp [ebp-0x38], eax` (`op == op`) on x86 and
     matching `i = 0` (`HCOP_CBA`) unconditionally when `s0 == cReg`.
  2. `HCBitSet_getNextUnset` (`hcBitSet.c`, `VA 0x451688`): uses logical `shr`
     (`data[ndx] >> ofs != 0xFFFFFFFFu`), which leaves `ofs` top zero bits when
     `ofs > 0`; harmless for its sole caller `minColor` (`length == nHardRegs
     <= 14`, single word, where `ofs + (32 - ofs) == 32 >= length` returns `-1`).

### `hcCompiler.c`, `hcPanicRegalloc.inl`, & `_grSignature` (`TI=0x18AA`) (session 4g)

- **`_grSignature` layout (`TI=0x18AA`)**: `grExecuteProgram` (`g_pixelPipeBytecode.c`)
  indexes `_grSignature` by raw byte offset (`+16..+48` + `texture[2][44]`).
  Dumping CodeView `TI=0x18AA` (`144` bytes) and `TI=0x18A7` (`44` bytes) from
  `sstGlobalTypes` recovered the exact byte order (`+16` `isValid`, `+17`
  `colorMask`, `+18` `depthTest`, `+19` `alphaTest`, `+20` `alphaFunc`, `+21`
  `depthMask`, `+22` `depthFunc`, `+23..24` `depthClampMin/Max`, `+25`
  `logicOpEnabled`, `+26` `logicOp`, `+27` `blendEnabled`, `+28..29`
  `srcFactor/dstFactor`, `+30` `flatShading`, `+31..39` stencil fields,
  `+40..41` `fogEnabled/monochromeFog`, `+42..48` `writeColor`,
  `constantWriteColor`, `applyColorWriteMask`, `dither`, `constantDepth`,
  `perPixelPack`, `screenRectGradients`), matching `s_mainBitCounts[56]` bit-for-bit.
- **`hcCompiler.c` static tables & x87 disassembly corrections**:
  - `swap[12]` (`VA 0x47E50C`), `rewrites[52]` (`VA 0x47E518`, `1352` bytes,
    `TI=0x1A7D`), and `dbl_48E070` (`VA 0x48E070` = `(double)0x7FFFFFFF`) dumped
    directly from PE `.rdata`.
  - In `pruneGraph` (`VA 0x459CCE..0x459D3B`), Hex-Rays inverted the x87 `fcomp`
    (`fnstsw ax; test ah, 1`) overflow checks at `hcCompiler.c:4422` and `4423`:
    the real machine code asserts `(double)spCost * (double)spillNeigh < (double)0x7FFFFFFF`
    and `(double)spillCost * (double)nNeigh < (double)0x7FFFFFFF`.
  - Three additional original Gerbera C quirks verified in x86 machine code and
    preserved faithfully:
    1. `findLabel` (`VA 0x453560`) returns instruction index `i >= 0` (or `-1`),
       while `validateProgramInternal` (`VA 0x4537CB`, `hcCompiler.c:542`)
       asserts `findLabel(lst, target) != 0` (`test eax, eax; jnz`), so a
       branched-to `LABEL` at instruction index `0` triggers line 542.
    2. `computeDominators` (`VA 0x454144`) ends its `do ... while` loop with
       `cmp [ebp+changed], 0; jz` (`while (!changed)`), terminating after the
       first forward RPO pass whenever `changed == 1`.
    3. `pruneGraph` (`VA 0x459D5F..0x459DDD`) updates `spillNode/spillCost/spillNeigh`
       *before* calling `HCRandom_get` and testing `spillNeigh * spCost == nNeigh * spillCost`,
       so ties (`<=`) always enter the tie-break RNG check.

### `api_opengl` & `core`/`grr` findings (sessions 5a-5b)

- **`tools/split_decomp.py` `_BOOL` string corruption**: the splitter's global
  `_BOOL -> int` regex also rewrote two assert string literals inside
  `generated/fragments/api/`: `"dataType == OGL_OUTPUTTYPE_BOOL"` (`oglGetter.inl`)
  and `"dataType != GR_DATATYPE_BOOL"` (`oglUtil.inl:1342`). Always verify
  assert strings containing `int` against `docs/libGLES_CM.dll` `.rdata`.
- **CodeView `LF_ENUMERATE` (`0x0403`) enum orderings**: `GR_STENCILOP_e` is
  `KEEP=0, REPLACE=1, INCR=2, DECR=3, ZERO=4, INVERT=5, MAX=6` (`0x152897`),
  `GR_BLENDFACTOR_e` is `ZERO=0..ONE_MINUS_CONSTANT_ALPHA=14, MAX=15` (`0x151abb`),
  and `GR_MATRIXMODE_e` is `MODELVIEW=0, PROJECTION=1, TEXTURE0=2, TEXTURE1=3,
  PALETTE0=4..PALETTE_LAST=12, MAX=13` (`0x152983`).
- **`grSetupTextureRectGradients` `_QWORD` store**: in `docs/libGLES_CM.dll.c`,
  `*(_QWORD *)&zVal[29] = (unsigned int)expr` and `*(_QWORD *)&zVal[31] = (unsigned int)expr`
  are 64-bit zero-extended stores into `int pairs[2][2]`, setting `pairs[coord][0]`
  and zeroing `pairs[coord][1]` (`preShift = 0`).
- **64-bit host portability in `oglContext_s` & `Gerbera_s`**: `oglCreateContext`
  pads `sizeof(oglContext_s)` and the 13 `oglMatrixStack_s` allocations (`2532` B)
  to `sizeof(void *)` alignment before carving `Gerbera_s` work memory, and
  `eglGetJmpbufSize()` returns `(sizeof(jmp_buf) + 7) & ~7` (`200` B on x86_64
  glibc vs `64` B on 32-bit x86).

## Strategy for full reconstruction

1. Port leaf libraries first (hg done, imath next) - no struct unknowns.
2. Reconstruct struct layouts for gerbera/hc/api/egl from field accesses in
   fragments (field names are preserved by the decompiler); cross-check
   offsets against the binary where the code uses raw offsets.
3. Port per-module with unit tests; the rasterizer (grr*) can be validated
   by rendering and diffing against a software reference.
4. Keep assert strings verbatim so a future dynamic run of the rebuilt DLL
   can be diffed against the original trace-by-trace.
