# PROGRESS.md - live status of the reconstruction

Last updated: 2026-11-22 (session 3)

## Legend

- DONE = ported into `src/`, compiles warning-free, unit tested
- SCAFFOLDED = fragment files exist, nothing ported yet
- input = reference material only, not a port target

## Module status

| module | original tree | functions (unique) | status |
|---|---|---|---|
| hg | renderer/libs/hg | 105 | **DONE** (session 1) |
| imath | renderer/libs/imath | 198 | **IN PROGRESS** - scalar + vector/matrix layers done (sessions 2-3) |
| gerbera/core | .../gerbera/core/implementation | 318 | **IN PROGRESS** - grDefs.h foundational layer done (session 3) |
| gerbera/core/rasterizer (grr) | .../core/implementation/rasterizer | 26 | SCAFFOLDED |
| gerbera/api_opengl | .../gerbera/api_opengl/implementation | 313 | SCAFFOLDED |
| gerbera/hc | .../gerbera/hc | 296 | SCAFFOLDED |
| egl | src/ (eglApi.inl, eglBase.inl, eglUtil.inl) | 63 | SCAFFOLDED |
| egl_symbian | inc/symbian/eglSymbian.inl | 39 | SCAFFOLDED |
| crt/platform | Symbian + MSL runtime | 44 | n/a (reference) |

Total unique functions in the binary: 1402 (1776 definitions, of which 374 are
per-translation-unit inline clones that were deduped).

## DONE session 3b (core: grDefs.h foundation)

1. **src/include/gerbera/grDefs.h + src/gerbera/grDefs.c**: surface descriptor
   (_grSurfaceDescriptor, from decompile cv_anon_14F9: rgbaOffsets[4],
   rgbaSizes[4], colorMask, color/depth/stencilBytes, padding[2], pixelFormat,
   paletteFormat), _grRect, _grStackAllocator, GR_IMAGEFORMAT enum (numeric
   values pinned by the 0x11A319 component-count bit table: comps
   {1,3,4,1,2,3,4}; index 6 unnamed so far), GR_DATATYPE + verbatim
   dataTypeSizes[12] table, grClearDescriptor / grSetDescriptorSizes /
   grSetDescriptorOffsets / grDescriptorsEqual (compares first rgba ints +
   colorMask + pixelFormat, faithful) / grGetDescriptorSize / grGetDescriptorOffset /
   grSetRectangle / grIsRectangleEmpty / grGetDataTypeSize /
   grGetImageFormatNumComponents / grIsStackValid / grStackGetFreeSpace /
   grStackInit / grStackGetPtr / grStackAllocate (4-byte round-up) / grStackFree.
2. **src/gerbera/grSmall.c**: grIsAligned (verbatim; pointer casts via
   uintptr_t for 64-bit hosts).
3. **tests/test_core.c**: 47 checks. Totals: 288 + 311 + 47 = 646 green.
4. Core conventions: grAssert/grDie pattern
   `if (!cond && grAssert(expr,file,line) != 0) grDie(0)`; descriptor clears
   zero ONLY rgbaOffsets[0]/rgbaSizes[0] + the byte fields + pixelFormat
   (faithful, not paletteFormat); grGetDataTypeSize_dataTypeSizes is `// weak`
   in the binary (define-once rule).

### Core next steps
- Pin GR_IMAGEFORMAT_6 / GR_PIXELFORMAT_MAX / GR_PALETTEFORMAT_MAX /
  GR_DATATYPE member names from oglSmall mapping functions + callers.
  GR_CAPABILITY values are NOW PINNED (session 3f).
- User clip planes (normalsToEyePal[17][0].originalVertex area +
  texCoordTransform[8] stride-80) need the transform-stack record layouts.
  _ITCTransform is NOW PINNED (session 3f, from ITCTransform_copy in
  _helpers_core): { e[4] uint, m[4][4], lsh[4][4], rsh[4][4], im[4] int64
  (row 3 * 0x10000 shifted), cl } - lsh/rsh = clamp(e-119, 0..31) /
  clamp(119-e, 0..31). The state->texCoordTransform records are 80-byte
  stride _IMatrix4x4 copies (user clip planes use slots 8..13);
  normalsToEyePal layout still open.
- grClearBuffers needs grFlushBufferClears -> grr rasterizer layer.
- grClearBuffers needs grFlushBufferClears -> grr rasterizer layer.
- grClearBuffers needs grFlushBufferClears -> grr rasterizer layer.
- Then grApiFast/grVertexPipeline -> pixel pipeline -> rasterizer.

## DONE session 3d (core: Gerbera_s + grApiSmall chunk 1)

1. **Descriptor refactor (packed bytes)**: rgbaOffsets/rgbaSizes are
   unsigned char[4] - grClearDescriptor/grDescriptorsEqual/grGetDrawBufferDescriptor
   move them as whole ints (decompile writes *(int*)a->rgbaOffsets = 0 and
   copies 4-byte chunks drawSurface <-> desc). pixelFormat sits at int 3;
   paletteFormat + padding[2] trail it. _grDrawSurface added (decompile
   cv_anon_1686): descriptor prefix (4 ints) + width (int 4) + height (int 5)
   + _grSurfaceBuffer buffers[3] ({data, stride}, strides at ints 7/9/11).
   Assert strings literally name drawSurface.width/.height and
   .buffers[GR_BUFFERTYPE_*].stride.
2. **grState.h (Gerbera_s)**: capabilities[54] (GR_CAPABILITY_MAX=54 from the
   range asserts; lights = caps 41..48 from grSetCapability `cap - 41`;
   per-cap numeric values still unpinned), scissor/viewport/surface/render
   rects, viewportScale/Offset as {int normal[3]}, zNear/zFar, useDithering,
   clear color/depth/stencil + clearInfoIndex, per-pixel fixed-function fields
   (alphaFunc/alphaReference, stencil 5-tuple, depthFunc, logicOp,
   blend factors, depth offset, color/depth/stencil masks), fog sextuple +
   fogAtZero, screenRectGradients, lighting dirty masks + normalsToEyePalDirty[9],
   cull quadruple, ambientColor (_XColor3). Enums: GR_WINDING (CW=0/CCW=1),
   GR_FUNC 0..8, GR_STENCILOP (ZERO=0, INVERT=5, MAX=6 - pinned by oglMapStencilOp:
   7680..7683 -> 0..3, 5386 -> INVERT=5, 0 -> ZERO=0), GR_LOGICOP (5376..5391 ->
   0..15, GL order), GR_BLENDFACTOR (0/1 + 768..776 -> 2..10, MAX=15),
   GR_FOGMODE 0..2.
3. **grApiSmall.c chunk 1 (31 functions)**: grUpdateCullMode, grSetDepthRange,
   grUpdateScissor, grSetScissor, grSetViewport, grCapChangesRejectAllState/
   PixelPipeline (bit tables verbatim from binary 5700/5706), grSetCapability,
   grGetCapability, grSetClearColor/Depth/Stencil, grSetCullMode, grSetFogMode/
   Density/Range/Color, grSetRectGradients, grSetGlobalAmbient, grSetAlphaFunc,
   grSetStencilFunc, grGetDrawBufferDescriptor, grSetDepthOffset, grSetStencilOp,
   grSetDepthFunc, grSetLogicOp, grSetBlendFunc, grSetColorMask, grSetDepthMask,
   grSetStencilMask; grTestCapability/grEnableCapability/grDisableCapability/
   grIsDepthOffsetEnabled (grUtil.h).
4. **grSmall.c**: grIsValidBool. **grDefs.c**: grIsRectangleInRange.
5. **Key faithful behaviors verified by tests**: scissor rect IGNORED while
   SCISSOR_TEST capability is off (surfaceScissor = full surface); render
   scissor = surface ∩ viewport, zeroed when empty; grSetClearDepth stores
   hgMulu64h(2*depth, 0xFFFFFFFF) (0.5 -> 0x7FFFFFFF); grUpdateDither needs a
   masked-in channel with size 1..7; XColor3_setx scales components x32.
6. **Material setters**: _grMaterial reconstructed from byte offsets
   (colors[4] _XColor3, XScalar shininess @int 12, alpha @byte 52);
   grSetMaterialColor/Alpha/Shininess/grGetMaterialShininess ported.
   GR_MATERIALCOLOR: AMBIENT/EMISSION named, values unpinned.
7. **tests/test_core.c**: +83 checks. Totals: 288 + 311 + 195 = 794 green.

## DONE session 3f (capability enum pinned via the EPOC decompile)

The user contributed `docs/libgles_cm.dll.epoc.c` - the stripped ARM device
build (all sub_XXXX, GNU C++, 1622 functions). Despite having no symbols it
resolves tables the WINSCW decompile truncates:

1. **GR_CAPABILITY values pinned**: WINSCW showed oglMapCapability's
   `values[24]` fully but truncated `keys` to its first element (2832).
   The EPOC build keeps both tables: keys = dword_35E6C[11] (44 bytes = 22
   uint16 keys, ascending), values = word_35E9A onward. 22/23 keys decode
   to GL tokens; combined with the values array the mapping is:
   ALPHA_TEST=1, DEPTH_TEST=2, STENCIL_TEST=3, BLEND=4, COLOR_LOGIC_OP=5,
   0x0BD0->6 (driver DITHER token; GL_DITHER is 0x0BD1), 0x0B60->7 (name
   unresolved), TEXTURE_2D=10, POLYGON_OFFSET_FILL=14, SCISSOR_TEST=15,
   CULL_FACE=16, LINE_SMOOTH=18, POINT_SMOOTH=19, key23-missing->20,
   LIGHTING=36, NORMALIZE=37, RESCALE_NORMAL=38, COLOR_MATERIAL=40,
   LIGHT0..7=41..48 (confirmed by EPOC `result + 41`), MULTISAMPLE=49,
   SAMPLE_ALPHA_TO_COVERAGE=50, SAMPLE_ALPHA_TO_ONE=51, SAMPLE_COVERAGE=52,
   MATRIX_PALETTE=53. Clip planes 0x3000..5 -> caps 30..35 (ogl space).
   LIGHT_MODEL_TWO_SIDE is internal (not a GL token) - value still guessed
   (39, flagged in grState.h).
2. **Cross-validation**: grCapChangesRejectAllState table {98306} = bits
   1/15/16 = ALPHA_TEST/SCISSOR_TEST/CULL_FACE - exactly the caps that
   reject all drawing. grCapChangesPixelPipeline {32766} = bits 1..14 =
   ALPHA_TEST..POLYGON_OFFSET_FILL. Both bit tables coherent with the pins.
3. grState.h enum rewritten with pinned values; all 818 checks stay green.
4. EPOC build noted as reference: same .data tables survive, useful wherever
   WINSCW Hex-Rays truncates arrays (treat as secondary, symbol-less).

## DONE session 3e (core: lights)

1. **_grLight record** (cv_anon_1748) reconstructed with named fields; the
   decompile's txMin/txMax/fx1 byte-offset aliases each map 1:1 onto them
   (mapping documented in grState.h). lightMask bits 1/2/3 = ambient/diffuse/
   specular nonzero (from grUpdateBaseColor |= (t&2)|(t&4)|(t&8)).
2. **12 light setters** ported: grSetLightPosition (positional =
   IFVector4_tov3 return = w!=0; directional lights get nVP normalized),
   grGetLightOrgPosition, grSetLightSpotDirection (stored negated +
   org float copy), grGetLightOrgDirection, grSet/GetLightColor (components
   0..2, SPECULAR=2 per the assert string), grSet/GetLightAttenuation
   (constant/linear/quadratic), grSet/GetLightExponent (XScalar roundtrip),
   grSet/GetLightSpotCutoff (raw degrees + max(cos,0) as XScalar).
3. **grVertexPipeline.c started**: grTestLightColor + grPrecalcLightColor.
4. **Faithful quirk verified**: XScalar_mul_sat(a,b) = (a*b)>>21, so small
   26-bit colors multiply to 0 (320x320 -> 0; the premultiply of dim lights
   vanishes by design). Tests use 2^21 colors for the nonzero path.
5. **tests/test_core.c**: +24 checks. Totals: 288 + 311 + 219 = 818 green.

## DONE session 3c (core: color + texture utilities)

## DONE session 3c (core: color + texture utilities)

1. **grRGBA.h/grRGBA.c**: grSetRGBA/grUnpackRGBA/grPackRGBA; word layout
   (a<<24)|(b<<16)|(g<<8)|r. GR_COLOR_MAX=4 pinned via
   `imageFormat == (GR_COLOR_MAX|GR_COLOR_A)` == GR_IMAGEFORMAT_MAX(7).
2. **grUtilSmall.c**: grIsValidDescriptor (17 verbatim assert sites),
   grPackColor, grMaskedDescriptorsEqual, grGetPackedColorWriteMask,
   grUnpackColor with every channel-size quirk: size 4..8 replicate,
   9..15 shift-down, size 1 -> hgBitMask32 (0/-1 -> 0/255 via unsigned char
   store), size 2 -> (16*((4c)|c))|(4c)|c, size 3 -> (c>>1)|32c|4c (c=1 -> 36),
   size 0 -> alpha defaults 255, others 0.
3. **grTexture.h/grTexture.c**: _grMipLevel (pixels, log2Dim[2], texFormat,
   initialized, pbuffer) + _grTexture (parmMask, filterMask[2], wrapMode[2],
   target, numMipLevels, palette, mipmap[11], cropRect, genMipMaps). Enums
   pinned: GR_TEXTURETARGET_2D=0/MAX=1 (MAX = unbound sentinel; init stores 1),
   GR_TEXTUREFORMAT 0..4 from oglMapInternalTextureFormat(6406..6410), paletted
   5..14, MAX=18 (init sentinel), GR_MAX_TEXTURE_MIPMAPS=11, DIMENSION_LOG2=10,
   MAX_TEXTURE_UNITS=2, filter bits FILTER=1/USE_MIPMAPS=2/FILTER_MIPMAPS=4,
   WRAPMODE_MAX=3, TEXCOORD S/T. Functions: grIsValidTexture, grUpdateTexture
   (mip-chain walk + parmMask synthesis, verified: chain break leaves
   parmMask=2), grInitTexture, get/set target, mip level access/format/
   initialized, wrap mode, gen mipmaps, crop rect (hgMemCopy 0x10), filter
   (MAG rejects mipmap bits). grGetTextureTargetDimensionality (2D only).
4. **grUtil.c additions**: grVector3iGet (+_grVector3i from cv_anon_16FD),
   grUnpackIndex, grSetIndices, grMaskedColorsEqual (ref channel = highest
   set mask bit; only masked channels compared against it).
5. **tests/test_core.c**: +65 checks. Totals: 288 + 311 + 112 = 711 green.
6. **Gerbera_s census** (for the context port): 119 distinct `state->` fields
   referenced across core fragments; heaviest users grVertexPipeline (247),
   grApiSmall (242), grApiFast (169). Full field list in this file's git
   history (session 3c survey).

## DONE session 3b (core: grDefs.h foundation)

## DONE session 3 (imath vector/matrix layer)

1. **New files** (`src/imath/`): `imInline.c` (XScalar 10.21 layer: mul/mul_sat/
   add/sub/abs/min/cmp/sign/tous/pow, reconstructed exp2, imRcpu packed
   reciprocal with intrcpLUT[310], XColor3/XColor3Acc, IFVector3 basics),
   `IVector3.c` (zero/set/init001p/normsub/length/clipperDot + rsqLUTm/rsqLUTa[256]
   embedded), `IVector4.c` (zero/set/set_noINF/inf/copy/testeq, IFVector4
   set/getf/copy/negate/setxv/setfv, smul/add/clip/wNorm/fixClipSpacePosition/
   stp/viewportTransform), `INormal3.c` (XNormal3: setf/setb/sets/setx/copy/
   negate/dot/normalize/init001p/n/accfv; rsqLUTs extern'd from IVector3.c),
   `IMatrix3.c` (XNormalTransform_mulNormal), `IMatrix4.c` (statics
   getScalar/setScalar/copy_column/rows_by_column/rows_by_scalar/subrcp/dp3h +
   copy/transpose/setfv/getfv/identity/I64_tof/mul/mulfv4/frustum/ortho/scale/
   translate/rotate/det2h/inverse/inversetranspose/dp4l/dp4l_fast,
   IFVector4_tov3/IFVector3_tov3/IFVector4_accfv, IVertexTransform_copy/mulxv4/
   mulxv4s).
2. **imApi.h extended**: full vector/matrix/XScalar/XNormal3/XColor3 struct
   defs + decls; `IMatrix4x4_inverse` HGint32 return; `IFVector4_dp4l` HGint32.
3. **IScalar.c additions**: expe (scalarToFixed(x,21) -> imMulAsr32Sat(v,
   774541002, 29) -> XScalar_exp2; constant verified vs decompile 23157),
   attenuateRcp (verified line-for-line vs decompile 23449), attenuateRsq.
4. **IMath.c**: IVector3_isValid / IVector4_isValid moved in (bsum & 0x20000000
   convention: at least one shared-exp mantissa must have the 2.30 top bit).
5. **Tests**: test_imath.c now 308 checks (XScalar roundtrips, vectors,
   normals, matrices incl. rotate 90/180 deg about z, ortho, inverse of
   diag(2,2,2,1), inversetranspose of rot90, attenuate clamps). Total
   288 + 308 = 596 checks, 0 failures, zero compiler warnings.

### Bugs found while verifying (fixed)

- **F_ADD swap path** (hgFloat.c): after the |a|<|b| swap the "equal
  magnitudes -> +0" shortcut compared the wrong pair (`ba == t` always true
  post-swap) - decompile compares original ba vs original aa. This broke
  every swapped-magnitude subtraction (IScalar_sub(0,2)==0), which killed
  ortho/inverse/inversetranspose through subrcp/rcp chains.
- **det2h**: stray `exp = 0` wiped the shared exponent before the clz
  renormalize -> every 2x2 determinant returned 0 -> inverse all zeros.
- Test-side: getScalar(c, r) is (column, row) - column-major storage; rotate
  products R[1][0]/R[0][1] were checked swapped; I64_tof input scale is 2^50
  (>= 2^51 trips the driver's own `e <= 127` assert - kept faithfully);
  IVector4_set with a packed exponent word > 0xFE inf-clamps (driver quirk,
  faithful); clip bits are 0x88/0x44 (main+guard, guardBits=0).

### Key conventions established (verified numerically)

- `_IMatrix4x4` is **column-major**: `v[i]` = column i, `m[c]` = row c;
  `getScalar(pS, c, r)` = element (row r, col c). mul(pD,pA,pB) = A*B;
  translate/scale/ortho/rotate all POST-multiply (M*T, M*R) like GL.
- rotate reconstructs Rodrigues: (k,j,m) pairs (1,2,0),(2,0,1),(0,1,2);
  p = (a_k*omc)*a_j and q = a_m*sin at scale 2^51, cols[k].m[j] = p+q,
  cols[j].m[k] = p-q, diag[j] = cos + a_j*omc*a_j; composed with
  rows_by_column (= M * col). The printed rotate decompile has overlapping
  stack slots (axis[] clobbered by tmp[]) - index choices reconstructed from
  scales + Rodrigues, structure/scales decompile-faithful.
- XNormal3_normalize output is 10.21 (1.0 = 0x200000); rotate consumes it.
- IScalar_getMantissa = (frac|implicit 0x800000 when e!=0) << 6, sign-applied
  (2.30 mantissa; 1.0 -> 0x20000000).
- attenuateRcp/attenuateRsq semantics: atten = ka0 + ka1*d + ka2*d^2;
  e<116 -> 0x7FFFFFFF (rcp would overflow); e>148 -> 0; else fixed21(rcp).

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
