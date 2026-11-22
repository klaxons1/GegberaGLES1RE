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

## DONE session 3l (grFlushBufferClears + grr clear family + grFill + grDitherColor + grClearBuffers)

- **grr clear family** (grrClear.inl 62-285, DLL 0043E220-0043EA50) ported to
  new src/gerbera/grrClear.c: grrClearDitheredColorBuffer (4 grDitherColor
  calls per row x0..x0+3, y0 incrementing; bpp 1/2/4 through the fill4x
  table), grrClearBuffer (fillFunctions1x[bpp-1] = grFill8/16/32Masked with
  NULL at slot 2 - a 3-byte surface hits the fillFunc assert 153, faithful;
  empty-scissor skip; stride == bpp*width -> single fill of width*height),
  grrClearColorBuffer (fillMask -1 unless the info mask is a strict subset
  of the surface mask, then grGetPackedColorWriteMask; grPackColor; dither
  path when useDithering && clearColor), grrClearDepthBuffer (clearDepth >>
  30-8*depthBytes for <4-byte depths), grrClearStencilBuffer (fillMask =
  stencilWriteMask), grrClearBuffers (bufferMask 1/2/4 -> color/depth/
  stencil).
- **grFlushBufferClears** (grApiFast.inl 26, DLL 00430AE0): asserts 33/34/48
  (hgInRange32(clearInfoIndex, 0, GR_CLEAR_BUFFER_SIZE) inclusive 4!), lock
  -> per-record grrClearBuffers -> unlock -> index 0. grLockSurface/
  grReleaseSurface (eglSymbian.inl 482/518) stubbed until the egl module.
- **grClearBuffers** (grApiSmall.inl 388, DLL 00443AD0) in grApiSmall.c:
  auto-flush when the queue is full (EPOC sub_221xx: index at +7472, records
  at +7476, 36 bytes each = 9 dwords; WINSCW reaches them as clearInfos[9*i]),
  asserts 395 (range 0..SIZE-1) + 399, record = surfaceScissor snapshot +
  colorMask/depthMask/stencilWriteMask + clearColor/clearDepth/clearStencil +
  useDithering. _grClearInfo added to grDefs.h (scissor x0,x1,y0,y1 at +0,
  colors +16..24, byte pack +28..32: bufferMask/colorMask/depthMask/
  stencilWriteMask/useDithering); clearInfos[GR_CLEAR_BUFFER_SIZE=4] in
  Gerbera_s after clearInfoIndex.
- **grFill.inl** (27402-27850) -> grFill.c: grFill8/16/32Masked (full mask
  routes to hgFill8/16/32) and grFill4x8/16/32 cyclic pattern fills (the
  dithered clear calls them with four per-row dithered colors). Endian
  join: hgGetEndianess() returns TRUE for BIG in this hg - the decompile's
  "!= HG_ENDIAN_LITTLE" big-endian branch is "== HG_TRUE" here (first port
  inverted it; caught by the dithered-clear test: swapped fill1 shorts).
- **grDitherColor** (grUtilSmall.inl 273, DLL 00439440) -> grUtilSmall.c
  with the DLL data table g_orderedDitherKernel[4] =
  { -7803290, 1140859562, -581579657, 295252923 } (0xFF88EE66, 0x440022AA,
  0xDD557... as printed): kernel[y&3], byte (x&3) via hgRor32(.., 8*(x&3))
  >> 24; channel = v - (v>>size) + (k>>size) per rgbaSizes, then grPackColor.
- **DESCRIPTOR LAYOUT CORRECTED** (was wrong since 3c): pixelFormat/
  paletteFormat are BYTES 12/13 inside int 3 with padding[2] at 14/15 - the
  descriptor view is 16 bytes, ints 0..3. Evidence: grClearDescriptor zeroes
  *(_DWORD*)&a->pixelFormat (int 3 wholesale); grIsValidDescriptor checks
  hgInRange32(pixelFormat, 0, 6) / (paletteFormat, 0, 2) / padding == 0 -
  all must stay inside ints 0..3 because the draw surface embeds the view
  at int 0 and reads width/height at ints 4/5 (grUpdateScissor raw dwords,
  grSetSurfaces copies cv_anon_1686 as 0x30 bytes; cv_anon_1686 = 48 bytes
  pinned by oglSetBuffers' _QWORD[15] scratch: two objects at bytes 24/72).
  _grSurfaceDescriptor + _grDrawSurface now share the 16-byte prefix;
  grrClear* take _grDrawSurface and use it AS the descriptor (cast macro
  GRR_DESC). GR_PIXELFORMAT_MAX 6 / GR_PALETTEFORMAT_MAX 3 now pinned by
  the assert strings (were flagged conservative).
- Fill dispatch tables (DLL 479264/479274) as FILL1/FILL4 macros with a
  double cast through void(*)(void) to keep -Wcast-function-type quiet.
- +121 checks (548 core, 1147 total): clear queue (record fields, 4-slot
  wrap + auto-flush, flush reset), RGB565/depth/stencil clears (0xFFFF /
  0x7FFF / 0xAB), masked clear (colorMask 0x3 -> write mask 0xFFE0, blue
  bits survive), scissor sub-rect (6-wide surface, rows/cols offset), the
  dithered pattern (row 0 flat 0x8410 over kernel 0xFF88EE66; row 1 =
  7c0f 8410 7bef 7bef 7c0f 8410 over 0x440022AA), grFill4x8 full+masked,
  grFill16/32Masked full+masked.
## DONE session 3k (grSetupCulling + GR_COMMAND + primitive-type/transform re-pins)

- **grSetupCulling** (31562-31699, asserts grApiFast.inl 108/113/157) ported:
  primitiveType validation (the long assert string verbatim, incl. the
  GR_PRI / MITIVETYPE line split), state->primitiveType store, vertex-data
  reject (cap VERTEX_ARRAY on && vertexArray[0].data == NULL -> STOP,
  TEXTURE_RECT bypasses - EPOC: data at state+316 = vertexArray[0]+8),
  reject-all computation (alpha-test with func 0, empty renderScissor) with
  the cached-verdict assert (state->rejectAllState == reject when not dirty),
  cull-mask-3 triangle reject, objectToClip = projection*modelview +
  modelview/projection _IVertexTransform refreshes on dirty, the user clip
  plane loop (planes 5..0: packed = plane+1+16*packed, per enabled plane
  inverse(projection * userClipMatrix[p]) applied to the original equation
  with the getFirstNonZeroSign sign dance: copy-or-negate, transform,
  re-negate), enabledUserClipPlaneIndices store, and the palette loop
  (matrixPalTransform[8..0] <- matrix[pal+4] when dirty bits 4..12 set,
  normalsToEyePalDirty[pal] = 1).
- **GR_COMMAND pinned**: RENDER=0, STOP=1 (EPOC grSetupCulling returns 1 on
  every STOP path, 0 at the end; the DrawTexOES caller checks result != 1).
- **GR_PRIMITIVETYPE re-pinned** (the old TRIANGLE=0 was a soft inference):
  POINT=1, LINE=2, TRIANGLE=3, TEXTURE_RECT=4. Evidence: the EPOC mode->type
  table dword_350DC = 0x02020201 (GL_POINTS->1, all line modes->2), the cull
  path tests primitiveType == 3 (triangle family), DrawTexOES passes literal
  4 (= TEXTURE_RECT) skipping the table, hex-rays left grUpdateVertexPipeline's
  comparison RAW (0) because 0 is NOT an enum member. grUpdateVertexPipeline's
  point-distance-attention gate stays `primitiveType == 0` VERBATIM - it is
  dead-end code: grGetPrimitiveVertices (called later in the same function)
  rejects 0, and grSetupCulling only ever stores 1/2/3/4. The old 3h test
  expectations (attn change flags the pipeline dirty) were pinned under the
  wrong enum and are adjusted: for any reachable type the attn cache path
  does not fire.
- **3i residual doubt resolved**: grState.h tags are CORRECT - modelview-
  Transform@+1664 (from matrix[0]), objectToClipTransform@+1864 (the mul
  result), projectionTransform@+2064 (from matrix[1]@+704, 80B stride -
  the WINSCW "matrix[6]" is a hex-rays overlay artifact). EPOC 21124-21131.
- **userClipMatrix renamed** (was origUserClipMatrices): it is a standalone
  6x80B array at EPOC +4968 written by grSetUserClipPlane (the modelview at
  specify time, 4168+80*plane) - NO alias with the matrix[] stack.
- **Caps**: GR_CAPABILITY_CLIP_PLANE0..5 = 30..35 added (were referenced
  numerically); rejectAllState field added to Gerbera_s.
- Probe findings: IVertexTransform_copy renormalizes identity 3.29 mantissas
  to 0x40000000 with exp 0x8f; IMatrix4x4_mulfv4 emits the matrix's 2.29
  mantissa domain (the transformed planes are 0x20000000/-0xE0000000, not
  float bits) - the EPOC plane records are 20B (5 dwords, EPOC culling
  copies dword0 raw and sign-processes dwords 1..4) vs my 16B _IFVector4
  following the WINSCW view; flagged as an open EPOC/WINSCW discrepancy.
  grSetUserClipPlane asserts plane range 10..15 and needs a non-empty
  renderScissor + rejectAllStateDirty=1 in fresh states (the cached-verdict
  assert compares the fresh reject against the stale cache).
- +29 checks (427 core, 1026 total): grSetupCulling reject paths, cached
  verdict, cull-mask reject, identity transform refresh, clip-plane packing
  (planes {0,2} -> 0x31) + identity-transformed equations, palette matrix
  refresh.

## DONE session 3j (light + grLightVertex + grApplyLighting)

- **_grLight record layout fully decoded** (WINSCW overlay-type math, all
  cross-checked): positionXYZ {_IVector3 e,m[3]}@0, spotCutoff@16 (raw
  IScalar degrees), lightMask@20, orgPosition@24 (16B), orgSpotDirection@40,
  spotDirectionNegated@60, colorsx[3]@72/84/96 (= my colors[] ambient/
  diffuse/specular), exponent@108, attenuation@112, cosSpotCutoff@128,
  premul_colors[3]@132/144/156, halfway@168, nVP@180, positional@192,
  xAtten@196, ndotL@200, specDot@204; stride 208 (EPOC grUpdateLights steps
  lights by 208). My struct field order differs but is field-based + 208B
  stride, so it is behaviorally identical - no re-layout done. The three
  per-vertex scratch fields (xAtten/ndotL/specDot) were ADDED to _grLight;
  grState.h now declares them + light()/grLightVertex/grApplyLighting
  prototypes; _grVertex gained weightIndices@112 (offset guess, read at
  28734, written by grQueuePrimitive 32194).
- **light()** (28554-28693) ported: bDirectional jumps straight to the color
  stage (xAtten untouched for directional lights!); positional: xAtten=0
  first, mask&0xE gate, lvec = IVector3_normsub(nVP, positionXYZ, pos) (L =
  normalize(lightPos - vertexPos)); spot (mask&0x10): spot =
  min(dot(nVP, spotDirectionNegated), 1.0), outside cone (spot < cosSpot-
  Cutoff) -> continue (vertex skipped); attenuation (mask&0x20):
  IScalar_attenuateRcp(lvec, attenuation); spot exponent pow + atten =
  spot*atten, atten==0 -> continue; assert sites 774/781/799/807 kept.
  Color stage: mask&0xC -> ndotL = dot(nVP, eyeNormal); signBit gate with
  two-sided bypass; mask&4 && !bDirectional -> ndotL = mul_sat(atten, ndotL);
  mask&8 -> halfway (positional only: nVP[2] += 1.0 then normalize halfway;
  directional reuses grUpdateLight's precomputed halfway), spec only when
  signBit(halfDot) == signBit(ndotL); shininess != 0 -> specDot =
  |halfDot| (pow shininess, mulu_sat atten if positional) else specDot =
  atten (not atten-scaled in that branch!).
- **grLightVertex** (28696-29135) ported: nSides = (lightingSideMask & 2);
  normal -> palette path (per-weight XNormalTransform_mulNormal + accfv +
  normalize, count = min(wt.size, idx.size, 4), indices byte per slot) else
  single mulNormal(normalsToEyeTransform, RESCALE_NORMAL cap); light() over
  l_directional (bDirectional=1) then l_positional (asserts 956/960);
  color accumulation in FOUR exact variants:
  * CM two-sided: acc = ambientLightSum; directional: NL>0 -> accF += colors[1]*NL
    (RAW light diffuse), spec>0 -> accFSpec += colors[2]*spec; NL<0 -> accB +=
    colors[1]*|NL| + spec; positional: premul[0]*xAtten into BOTH accs, then
    colors[1]*NL front / |NL| back, spec -> premul[2]*spec (front+backSpec);
    epilogue scaleColor(acc, cmColor) + addColor(material.colors[3]) +
    addAcc(spec) + toRGB per side.
  * CM single-sided: directional gated NL>=0 (diffuse raw + spec premul[2]);
    positional: accF += colors[0]*xAtten (RAW light ambient, direct madd)
    then NL>=0 gate; same epilogue + the b=r quirk (input.rgb[0].b =
    input.rgb[0].r clobbers final[0][2..3]; final[1][2] = final[0][2]).
  * base two-sided: acc = baseColor; everything premul: NL>0 -> accF +=
    premul[1]*NL + premul[2]*spec (same acc), NL<0 -> accB += premul[1]*|NL|
    + premul[2]*spec; positional premul[0]*xAtten into both; toRGB both rows.
  * base single-sided: NL>=0 gates, positional premul[0]*xAtten direct madd;
    toRGB front only + b=r quirk. Alpha: CM = final[1][2]&0xFF00 (== the
    fetcher's alpha), base = material.alpha&0xFF00 (EPOC material+52).
- **grApplyLighting** (29139-29223) ported: transformVE != 0 -> per-vertex
  state<2 raised to EYE_SPACE via mulxv4s/mulfv4s(modelview) (assert 1286),
  then EYE_SPACE -> EYE_SPACE_CARTESIAN (tmp copy, w slot = fog z via
  IFVector4_getf(tmp,2), IFVector4_tov3), grLightVertex, walk list until
  0xFF byte; transformVE == 0 -> lights in place (assert 1311 GR_FETCH_CHECK).
- **XColor3Acc accumulators are HGint64[3] (r,g,b arrays), NOT scalars** -
  scalars made madd clobber neighboring locals (caught by the two-sided
  probe: accFront read back as the accBack product). All acc args pass the
  array (no &).
- Probe-pinned outputs (tests/test_light_vertex, +35 checks = 399 core,
  998 total): directional front -> final ff00 x3 (b=r quirk gives sec =
  ff00 ff00 ff00 via the u16/int view packing, alpha 0000); two-sided back
  -> front 0000 / back ff00 x3, ndotL = -2^21; positional + transformVE ->
  state 3, ndotL = xAtten = 2^21, final white, lightMask 0x4 (atten
  (1,0,0), spotCutoff 180); CM half-intensity color -> 8000 x3, alpha
  ff00. Probe gotchas: grSetLightAttenuation needs NORMALIZED IScalars
  (IScalar_ftos - raw 0x200000 asserts IScalar_getMantissa==0), an unset
  attenuation (0,0,0) drives atten to INT_MAX via rcp(0) (faithful);
  printf args evaluate before the called function again (grUpdateLights
  results printed stale - call into a variable first).

## DONE session 3i (grTransformVertex + matrix palette skinning + clip codes)

- **grTransformVertex** (27926-28117) ported with all 11 assert sites:
  float path IFVector4_setfv(state 0), fixed path builds the 16.16 int view
  {x,y,z,w} in the position union's fixedView (x in the float-view e slot;
  INT16/INT8 <<16, FIXED raw; z/w default 0/0x10000), matrix-palette skin
  (weights via grFetchWeightAndIndex, count = min(wt.size, idx.size, 4),
  IVertexTransform_mulxv4s/mulfv4s per nonzero weight, IFVector4_accfv into
  position, state 2 = EYE_SPACE, clipPosition = projection), else
  objectToClip direct. Ends with grComputeVertexClipCodes.
- **_grVertex updated**: clipPosition@72 / clipMask@108 / stateMask (offsets
  flagged guesses - named-only), fixedView[4] (union now 20 bytes,
  sizeof(_grVertex) == 128 asserted).
- **clip codes decoded** (WINSCW + EPOC agree): enabledUserClipPlaneIndices
  packs (plane+1) per enabled plane base-16 (planes walked 5..0); the
  consumer reads plane digit-1 (EPOC base+4728+20*digit-20) and ORs the
  outside test into **bit digit+9**. IVector4_clip guard-band codes: bits
  0/1 = z beyond -/+w, 6/7 = x, 8/9 = y; bits 2-5 = the >>guardBand versions
  (guardBand=2 at the call site).
- **grFetchWeightAndIndex** (27862-27923): vertexArray[3] = weights (FLOAT
  iftos / FIXED fixedToScalar(16), size 1..4), vertexArray[4] = matrix
  indices (UINT8, each clamped to 8, packed one byte per slot, first index
  in the low byte).
- imath: IVertexTransform_mulfv4 (_IVector4-out: identity fast path
  renormalizes to shared exponent; dp4l rows + w passthrough when cl byte7
  == 0x80) and IVertexTransform_mulfv4s (_IFVector4-out per-component
  exponent bytes; w passthrough when cl byte7 == 0x80 AND w==16.16 1).
  mulxv4s existed already. WEIGHT_ARRAY=27 / MATRIX_INDEX_ARRAY=28 caps are
  flagged guesses (unpinned internal gaps).
- grSetupRendering/grCleanupRendering surveyed - they need the HC compiler
  + grr layer (grFlushBufferClears, grrBeginRendering/UpdateState/
  EndRendering, grCheckPixelPipeConsistency, grGetBackend/HC_extraStackSpace,
  grScratchGrowMemory, grStackAllocate, grVertexCacheInvalidate); deferred
  until the grr/HC foundations exist.
- +33 checks (364 core, 963 total).

## DONE session 3h (ITC transform use + vertex fetch stages + grUpdateVertexPipeline)

- **GR_CAPABILITY_FOG = 7** - the "unresolved" 0x0B60 key was GL_FOG all
  along (EPOC grUpdateVertexPipeline gates fog selection on capabilities[7],
  byte 60). Also pinned from the EPOC byte guards: VERTEX_ARRAY=21 (74),
  COLOR_ARRAY=23 (76), TEXTURE_COORD_ARRAY_0/1=25/26 (80/81),
  POINT_SIZE_ARRAY=29 (82); NORMAL_ARRAY=22 flagged guess. GR_FOGMODE
  remapped: 0=LINEAR (FAST/LINEAR by fogOORange), 1=EXP, 2=EXP2 - the old
  NONE/LINEAR/EXP mapping was WRONG.
- **_grVertexArray record** (stride 20, EPOC base +308): stride@0, size@4
  (u8), type@5 (u8; EPOC has it at 6 - build padding difference), data@8,
  binding@12, origPointer@16. Records: 0=vertex 1=normal 2=color 5=point
  size 6/7=texcoord units.
- **_grVertex (cv_anon_1712, 128 bytes)**: index@0, position@4 (union),
  color@40 (union: u16 final[2][4] rows vs XColor3-int view, alpha shared at
  final[1][2]), texCoord[2]@56/64, normal@92, pointSize@104; stateMask top 2
  bits = position state (grGetVertexPositionState = stateMask >> 14).
  Vertex buffer = (char*)renderStack + 1332 (renderStack[166].whichPool).
- **_ITCTransform use**: all outputs are 10.21 (the <<5/32x slow paths and
  the lsh/rsh fast paths agree); grTexCoordDivByQ divides 10.21 x,y by the
  10.21 q (passthrough at q==2^21 or 0; imRcpu rcp, faithful 1-LSB loss).
  Ported mulxv2/mulxv4/mulxv/mulfv2/mulfv4/mulfv (mulfv z-output never
  produced). IVertexTransform_zmulfv4/zmulxv4 added to imath.
- Ported all fetch stages (8 color, 5 normal, 7 texcoord, 2 point-size),
  fog (FAST/LINEAR/EXP/EXP2 via grGetFogZ/grGetVertexPositionState),
  grGetPrimitiveVertices (returns primitiveType; POINT=1 pinned twice),
  grGetVertexArrayPointer/Binding/Reset, grUpdateVertexPipeline (the stage
  dispatcher; textureUnitEnabled[2] = EPOC bytes 48/49 gate per-unit
  texcoord stages + the default-texcoord transform; defaultTexCoord is a
  union: rows 0,1 = float s,t,r,q, row 2 = 16.16-int outputs {x0,y0,x1,y1}).
- +51 checks (331 core, 930 total).

## DONE session 3g (transform stack + user clip planes + light update)

The EPOC decompile pinned the whole transform arena (Gerbera_s byte offsets,
self-consistent): matrixDirtyMask +112, matrix[13] +624 (stride 80, slots
0=MODELVIEW 1=PROJECTION 2,3=texture 4..12=matrix palette), modelview/
objectToClip/projectionTransform +1664/+1864/+2064 (_IVertexTransform,
stride 200), matrixPalTransform[9] +2264, texCoordTransform[2] +4064
(_ITCTransform, stride 132!! - lsh/rsh are bytes, im is int[4]; the earlier
int64-im guess was WRONG; the WINSCW `texCoordTransform[11*unit]` indexing =
11 12-byte slots per unit), normalsToEyeTransform +4328 / normalsToEyePal[9]
+4368 (_XNormalTransform stride 40), userClipPlanes[6] +4728,
origUserClipPlanes[6] +4848, origUserClipMatrices[6] +4968,
l_directional/l_positional[9] +5448/+5484, material +5520, ambientLightSum
+5588, lights[8] +5712 (stride 208: positionXYZ +0, spotCutoff +16,
lightMask +20, orgPosition +24, orgSpotDirection +44, spotDirectionNegated
+56, colors[3] +72, exponent +108, attenuation +112, halfway +168, nVP +180,
positional +192), enabledUserClipPlaneIndices +7620. The WINSCW clip-plane
storage expressions (`texCoordTransform[8][0].padding[80*plane-1]`,
`normalsToEyePal[17][0].originalVertex + plane`) are Hex-Rays artifacts:
EPOC = state+4168+80*plane / state+4648+20*plane with RAW plane 10..15 ==
origUserClipMatrices/origUserClipPlanes[plane-10].

- Ported grIsMatrixDirty/grSetMatrixDirty (grUtil.c), grSetMatrix,
  grSetUserClipPlane, grGetUserClipPlane (grApiSmall.c), ITCTransform_copy
  (new grTransform.c), grUpdateLight, grUpdateAmbientLightSum, grUpdateLights
  (grVertexPipeline.c). GR_MATRIXMODE + GR_CLIP_USER0 enums added.
- _grLight: halfway[3] added; lightMask bits 16=spot(cutoff!=180), 32=attn,
  64=directional; grUpdateLights partitions lights into l_directional/
  l_positional (NUL-ended, 9 slots), refreshes premul colors vs material,
  ambientLightSum = ambientColor + sum(l_directional ambient colors[0]),
  baseColor = ambientLightSum*material.ambient + material.emission, returns
  "any positional light on" (transformVE bit for grUpdateVertexPipeline).
- IScalar_immediate(f) = raw float bits: spotCutoff default 180 stores as
  0x43340000, exactly the EPOC compare constant.
- +61 checks (280 core, 879 total). enabledUserClipPlaneIndices packs the
  enabled-plane list as base-16 digits (plane+1 per digit, grSetupCulling).

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
