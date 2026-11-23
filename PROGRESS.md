# PROGRESS.md - live status of the reconstruction

Last updated: 2026-11-22 (session 4e)

## Legend

- DONE = ported into `src/`, compiles warning-free, unit tested
- SCAFFOLDED = fragment files exist, nothing ported yet
- input = reference material only, not a port target

## Module status

| module | original tree | functions (unique) | status |
|---|---|---|---|
| hg | renderer/libs/hg | 105 | **DONE** (session 1) |
| imath | renderer/libs/imath | 198 | **IN PROGRESS** - scalar + vector/matrix layers done (sessions 2-3) |
| gerbera/core | .../gerbera/core/implementation | 318 | **DONE** (sessions 3-3z) - all functions ported, 1088 checks green |
| gerbera/core/rasterizer (grr) | .../core/implementation/rasterizer | 26 | **DONE** (sessions 3-3z) - clears, blit, texture rect |
| gerbera/api_opengl | .../gerbera/api_opengl/implementation | 313 | SCAFFOLDED |
| gerbera/hc | .../gerbera/hc | 296 | **IN PROGRESS** - opcode + interpreter + memory/arena + backendInfo + **BOTH x86 & ARM JIT backends** done (4a-4e, 1498 core checks); HC_compile & hcEmulator next |
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

## DONE session 3v (grPipelineCache.c: compiled-program cache)

- **grPipelineCache.c (new, Makefile += )**: all 9 grPipelineCache.inl
  functions + the 3 grSmall.c helpers. grCacheMatchSignatures (assert 44,
  hgMemCompare 0x28 = 40-byte signature); grCacheGetOldestUsed (assert 58,
  min timeStamp over firstUsed); grCacheChangeList (asserts 140/146 - the
  decompile's prev[1]/colorsx writes = the standard entry->prev->next /
  entry->next->prev relink: unlink from *oldList (head-ownership asserted),
  relink at *newList head); grCacheMergeFreeBlock (assert 177, TWO passes -
  merge entry with an adjacent free block either side, survivor keeps the
  free list); grCacheRemove (assert 223: dataSize != 0 -> free list + merge,
  else -> unused pool); grCacheDefragment (asserts 248/249/278: while >= 2
  free blocks, take the lower-offset one, move the USED block that follows
  it onto it (memmove + grICacheFlush), advance + merge); grCacheInit
  (asserts 349/350/351: entries[0] = the whole-arena free block, entries
  1..n-1 chained via ->next as the unused pool); grCacheFind (assert 419:
  signature match -> timeStamp refresh + gradientMask out + arena ptr);
  grCacheInsert (asserts 463/464/503/506: rejects dataSize > arenaSize or
  entryCount < 2; LRU-evicts while free < dataSize || no unused || no free;
  best-fit free block, TAIL allocation (offset = best->dataSize - dataSize,
  dst = arena + best->dataOffset + offset); leftover stays free, exact fit
  moves the block free->unused). Helpers: grCacheGetFreeMemory (sum of free
  blocks), grCacheSelfCheck (DLL empty), grCacheFindFreeEntry (first fit).
- **grICacheFlush ported** (grSmall.c; DLL 42318-42321): EPOC = User::
  IMB_Range; hosted no-op.
- **grState.h**: _grCacheSignature (40B cv_anon_1725), _grCacheEntry
  (sig + gradientMask + timeStamp + dataOffset + dataSize + prev/next;
  64B in the 32-bit DLL), _grPipelineCache (8 words: firstUsed/firstFree/
  firstUnused/entries/arena/arenaSize/entryCount/timeStamp; 32B in the DLL);
  state->pipelineCache/scratchMemory/scratchMemorySize/setjmpConfig (offsets
  UNPINNED).
- **32-bit sizeof trap (REAL BUG avoided)**: the decompile's hgMemSet(cache,
  0, 0x20) and hgMemSet(entries, entryCount << 6) are the 32-bit struct
  sizes; on x86-64 they under-zero the structs (stale arenaSize/entryCount
  -> the eviction loop removed a NULL oldest). Ported as sizeof(*cache)/
  sizeof(*entries) with the DLL literals in comments.
- **tests/test_core.c test_pipecache (+40)**: init chaining, find-miss,
  insert tail-allocation (A@192, B@128, C@0 exact-fit frees the block),
  find-hit (ts bump + gradientMask), LRU eviction (A out, its tail block
  reused for the new insert @192), explicit removes with adjacency merge
  (64 -> 192 single block), defragment no-op on one free block, oversize +
  zero-arena rejects.

## DONE session 3u (grTessellator.c: perspective subdivision)

- **grTessellator.c (new, Makefile += )**: grFracDiv (DLL 26703-26725, asserts
  grTessellator.inl 49/50): a/b as an IUNIT fraction via imRcpu(b) + hgMulu64 +
  hgToSigned64 + hgLsl64h(s, shift) / 2 - the /2 FLOORS (grFracDiv(1,2) =
  ONE/2 - 1; inherited from the DLL). grLerp (DLL 26728-26742, assert 70):
  ((ONE - f)*a + f*b) >> 29. grClassifyTriangle (DLL 26745-26841, assert 92):
  extent = max bbox span (20.12); < 4px -> AFFINE(0); pixel-rounded bbox
  missing the scissor -> DISCARD(2); else w test on normal[2] (the THIRD
  clip-factor slot = per-vertex clip w): PERSPECTIVE(1) iff maxW'*(4096-ext)^2
  >= minW*(4096+ext)^2 with maxW' = max(maxW, minW + minW>>4) (1/16 w
  tolerance; equal w -> AFFINE). GR_TRIANGLESTATUS_e added to grState.h
  (PERSPECTIVE = 1 flagged guess - the decompile names only AFFINE/DISCARD).
- **grSplitEdge** (DLL 26844-26944, asserts 195/198): extent from hgDiff32
  (abs diffs); tiny edge or affine-safe w -> return 0 (unless forceSplit);
  swaps v0/v1 so w0 >= w1; w0' = max(w0, w1 + w1>>4); sub-pixel position
  fdw = grFracDiv(w0'-w1, w0'+w1), dist = min(fdw*extent >> 29, 0x2000) (2px
  cap), t17 = low32(dist << 16), factor1 = max(2*t17 - ONE, 0); fdw2 =
  grFracDiv(w0large, w0large+w1small) in [0.5, 1]; split factor t =
  IUnitScalar_lerp(ONE/2, fdw2, factor1) in [0.5, 1]; dst->normal[2] =
  grLerp(w0large, w1small, fdw2) (= 2*w0*w1/(w0+w1) harmonic mean - the
  perspective-correct w at the biased midpoint); grLerpClipFactor on the
  normal[0..1] clip-factor views by t; screen = midpoint & 0xFFFFFF00
  (256-aligned), z = average. Note: when dist caps at 0x2000 the factor
  saturates to fdw2 (the whole edge is perspective-critical).
- **grTessellateTriangle (static)** (DLL 26947-27246, asserts 283/336/337/350/
  499/376 canaries): classify; DISCARD -> return; PERSPECTIVE -> split edges
  j = 2..0 into slots 68 + 3*level + j (forceSplit escalates after the first
  split; alignment asserts with emptyBits = 7 - level are vacuous-true given
  the &~0xFF store but kept verbatim; dst->index = -1, stateMask = 79,
  grInterpolateVertexGradients per split); numSplits 3/2/1 -> recurse via the
  split tables, else draw (level != 0 recomputes + roundTriangls the area,
  level 0 keeps the caller's area; zero-area pieces skipped).
- **SPLIT TABLES DECODED** (data.fragments.c 68-78): the DLL's three "arrays"
  (splitIndexTable@S, byte_4751D0@S+1, byte_4751D1@S+2) are three overlapping
  stride-3 views of ONE triple array starting at S: child i = (A, B, C) =
  (S+3i, S+3i+1, S+3i+2). Full bytes: {00 04 06 | 04 05 06 | 04 01 05 |
  05 02 06 | 00} = children (v0, m01, m20), (m01, m12, m20), (m01, v1, m12),
  (m12, v2, m20) - bit 2 set = split vertex relative to base 68+3*level,
  else original vertex slot; all four CCW-consistent. nextLut = {1, 2, 0}.
- **grTriangleTessellator** (DLL 27249-27359, asserts 512/547/549-552/555/556):
  IVector4_wNorm seeds the per-vertex w into normal[2] (wNorm normalizes the
  three clip w's to a common exponent: equal-e test data gives w = m[3]>>1);
  origIndices == tri->indices -> seeds weights (1,0)/(0,1)/(0,0); else
  verifies the caller's weight invariants (orig0 = (1,0), orig1 = (0,1),
  orig2 = (0,0) when distinct - set by the clipper/line/point paths); then
  grTessellateTriangle(state, tri, origIndices, 0). grrStubs.c tessellator
  stub REMOVED.
- **Vertex budget confirmed**: split slots 68..91 (68+3*7+2 = 91) = the top 24
  slots of the 28-slot temp region (64..91); clipper temps start at 92 -
  disjoint ✓.
- **tests/test_core.c test_tessellate (+49)**: grFracDiv floors (1/2, 1/4, 0),
  grLerp exact, classify (AFFINE small/equal-w, DISCARD outside, PERSPECTIVE
  var-w), grSplitEdge (split: mid 26px/10px, w 715827883, factors 2/3-1/3;
  flat -> 0; tiny -> 0; forced-but-unjustified -> 0), e2e perspective
  recursion (wNorm w's, 6 split records all 256-aligned with stateMask 0x2F,
  last piece scans 7 rows from y=10 with words 26|7...26|2), e2e affine
  (equal w: no splits, single draw, 31 rows). The dispatch test in
  test_scanconvert updated: the PERSPECTIVE_CORRECTION path now runs the REAL
  tessellator (numScanlines > 0 instead of the stub's no-op).

## DONE session 3t (grDrawClippedLine + grDrawClippedPoint)

- **grRasterizer.c += grDrawClippedLine** (DLL 26391-26556, asserts grRasterizer.inl
  931/933/936/938/943/945/947/1067/1073). Geometry (all recovered, hand-verified):
  lineWidth = LINE_SMOOTH ? 0x10000 : max((state->lineWidth + 0x8000) & 0xFFFF0000,
  0x10000); steep = |dx| < |dy| (dx/dy = screenPosition deltas); slope = (imRcpu(
  |major|).mantissa * minor) >> (shift + 19) & ~0xFF (2.14 px/row on the minor
  axis); hw = lineWidth >> 5 (half-width, 0.12 px); PERPENDICULAR offset = hw
  (steep: offX, non-steep: offY = -hw), MAJOR half-pixel snap = 2048 (steep:
  snapY, non-steep: snapX), SLANT term = slope on the minor axis (steep: snapX,
  non-steep: snapY); the slant/snap SIGN flips when the far endpoint sits at
  larger major coordinate (the decompile's dstPos[13]/dstPos[11] swap - only
  the slope/snap terms negate; rec0/rec1 are NOT swapped, only the slope+snap
  pair); the weights set origIndex0 -> (1,0) / origIndex1 -> (0,1) in
  normal[0..1] BEFORE the swap test. Corner loop k=3..0: record slots 64..67
  (bytes of 1128415552 = 0x43434340), src = (k&2) ? rec1 : rec0, copies
  normal[0..1] + clipPosition, stateMask = 79 (0x4F: COLOR_COMPUTED|2D|3D|
  TRANSFORMED|VIEWPORT), screenPosition = snap? + src + offset*sign[k] with
  THE SAME sign table for both axes (sign[] = { -1,+1,+1,-1 } = the DLL
  ySign; using xSign degenerates the non-steep end edges - found by probe),
  z = src z. grInterpolateVertexGradients per corner, then two quads (64,65,66)
  and (66,67,64) via grDrawClippedTriangle(., ., origIndices, 8), assert 1073
  (both triangles same area), grDisableCapability(GR_CAPABILITY_UNUSED) after.
  RECONSTRUCTION: dstPos[4].shift/dstPos[6].rcp stores are Hex-Rays-lost;
  recovered as the half-width (matches the visible steep-branch store, keeps
  corners 256-aligned and the quad a parallelogram). Zero-length line (dx==0 &&
  dy==0) draws NOTHING (decompile if (!dx && !dy) skip).
- **grRasterizer.c += grDrawClippedPoint** (DLL 26559-26698, asserts 1106/1107/
  1108/1259-canary). size = POINT_SMOOTH ? 0x10000 : (POINT_SIZE_ARRAY &&
  vertexArray[5].data ? record->pointSize(+104) : state->pointSize); distance
  attenuation (computePointDistAttn): size = scalarToFixed(fixedToScalar(size,16)
  * attenuateRsq(IVector3_length(&record->position), &state->pointDistAttn), 16)
  (position union alias for the eye-space xyz); hgClamp32(size, pointSizeMin,
  pointSizeMax), hgMin32(size, 0x400000), size = max((size + 0x8000) &
  0xFFFF0000, 0x10000); odd = (size >> 16) & 1 (odd pixel size); half =
  (size >> 5) & 0xFFFFF800; center = (screen - 2048) rounded to the pixel grid:
  odd -> (v & ~0xFFF) + 2048, even -> (v + 2048) & ~0xFFF; corners = center +
  half * sign (xSign for X, ySign for Y here - the POINT uses the standard
  tables), z copied; stateMask 79; grInterpolateVertexGradients (weights 0,0
  -> orig record); POINT_SPRITE (cap 20, flagged guess slot) && textureUnit-
  Enabled[u] -> texCoord[u] = xTexCoord/yTexCoord[k] ({0,0,0x1FFFFF,...}).
  Two quads like the line; NO assert-1073 check, NO UNUSED disable (those are
  line-only).
- **grState.h**: GR_CAPABILITY_KEY23 -> GR_CAPABILITY_POINT_SPRITE = 20 (flagged
  guess: slot pinned by grDrawClippedPoint's grGetCapability; the ogl
  point-parameter path confirms in the api session); state->lineWidth/pointSize/
  pointSizeMin/pointSizeMax (offsets UNPINNED - setters land with the api
  session; values 16.16).
- **tests/test_core.c test_linepoint (+53)**: horizontal line (10,15)->(30,15)
  lineWidth 2px: corners 64=(8.5,16) 65=(10.5,14) 66=(30.5,14) 67=(28.5,16)
  (with the slant +-1px on the non-major axis), stateMask 0x2F, corner colors
  from the barycentric weights, second triangle scans 2 rows from y=14
  (27|3, 25|5); vertical line (20,10)->(20,30): corners on x=[19,21], rows
  trimmed to 10 from y=19 (zero-width rows drop); zero-length line no-ops;
  point (20,20) size 3px (odd snap, [18,21] box, z copied, color from orig
  record 2), even size 2px ([19,21] box), point-sprite texcoords per unit.
- **Sign-table lesson**: grDrawClippedLine multiplies BOTH axis offsets by the
  same sign[] table (= ySign); grDrawClippedPoint uses xSign/ySign. The
  decompile's single `sign[dstPos[14].shift]` read covers both axes - copying
  the point's two-table pattern silently degenerates non-steep quads.

## DONE session 3s (grInterpolateVertexGradients)

- **grRasterizer.c += grInterpolateVertexGradients** (DLL 25530-25899,
  asserts grRasterizer.inl 251/253/254/255/259/260/261/277/303/332/341/343/
  371/373/384/386). The clip-generated vertex carries its barycentric
  weights in normal[0..1] (clipFactor alias; c = IUNITSCALAR_ONE - a - b,
  not stored); g = (a*t0 + b*t1 + c*t2) >> 29 (imLsl64h_0_31(sum, 3) ==
  sum>>29 - the weights are IUNIT 1.0 = 2^29). Attribute walk: texcoords
  (32-bit ints, unit 1..0 outer via mask bits 192<<2u, coord 1..0, record
  +56+8u+4c), fog (u16 record+54 = final[1][3], mask bit 2, also asserted
  in [0,65535]), colors (u16 record+40+8*side+2*comp, mask 4<<comp for
  comp 2..0, asserted in [0,65535] + min/max), alpha (u16 record+46 =
  final[0][3] ALWAYS the front row regardless of side, mask 0x20). stateMask
  &= ~0x40 |= 0x20 happens BEFORE the branch, both paths. Trivial case
  ((b|a) & 0x1FFFFFFF)==0: rec = (a==1) ? rec0 : (b==1) ? rec1 : rec2,
  wholesale color-struct (16B) + texCoord copy; assert 277 (a==1||b==1||
  (a==0&&b==0)). Depth (mask bit 0) NOT handled here (per-scanline gradient).
  grrStubs.c stub REMOVED.
- **tests/test_core.c test_interpolate (+24)**: exact-weight interpolation
  (0.5/0.25/0.25 over R/G/B/fog/alpha/texcoords, front+side=1 rows, alpha
  from front), stateMask 0x4F -> 0x2F flip, mask-gating (no bits -> nothing
  touched), trivial copies for all three winners.

## DONE session 3r (scan conversion + real triangle draw dispatch)

- **grRasterizer.c += grDivsu / grScanConvertWithXScissoring / grScan-
  ConvertWithoutXScissoring / grScanConvertTriangle** (DLL 24922-25331,
  asserts grScanConvert.inl 29/55/72/73/95/96/118/135/136/158/159/249/250/
  252-257/359/360/361). POINT FORMAT PIN: cv_anon_16FD = &screenPosition[0],
  .x = screenPosition[0] (X pixel), .y = screenPosition[1] (Y pixel);
  screen coords are 20.12 = pixel<<12 with the low emptyBits(=8) bits zero
  (viewportOffset = pixel<<4 (16*x0+8*w+8), scale = w<<6, hgMul64h -> pixel<<4,
  <<xyShift(8) -> pixel<<12; CONFIRMS 4096 = 1px and grSubpixelTrunc = >>12;
  the 3q note "pixel<<8" was wrong, fixed above). cv_anon_1787 = {xStart[2],
  xAdder[2]} pair: slot0 = LEFT edge, slot1 = RIGHT edge.
- grScanConvertTriangle algorithm (all integer-exact, verified by hand-run
  probe): deltas d10/d20/d21 = (vJ-vI)>>emptyBits (12.4 pixels); heights
  topHeight = min(v1y,y1)-min(v0y,y1), btmHeight = min(v2y,y1)-min(v1y,y1)
  (y-clamped against scissor y1 only); longEdgeAtV0 = 4*v0x + (fracV0y*
  slope02 >> 12) where fracVy = 4095-(vy&0xFFF), slope02 = grDivsu(d20x<<14,
  d20y) (2.14 px/row); longEdgeAtV1 uses the RAW delta (v1y-v0y) NOT top-
  Height; longMid = longAtV1 - (fracV1y*slope02>>12) = long-edge x at v1;
  side = sign(4*v1x - longMid) (1 = v1 left of long edge -> long edge is
  the RIGHT edge); per-half short edges only when that half's height > 0
  (this is division-safety: horizontal short edge <=> zero height, no
  grDivsu(b<=0) assert); y0-clip advances BOTH edge pairs of a half and
  shrinks its height when the half starts above scissor y0 (nested: bottom
  half advance only inside the top-half block); emit loop writes left |
  numPixels<<16 per row; empty rows trimmed from the END then the FRONT
  (yStart++ per front row); stores yStart(+188)/numScanlines(+180)/
  scanlineInfo(+184) into the pixel pipe. SCANLINE BUFFER = renderStack+
  15668 (&renderStack[1958].whichPool), up to 2048 words, ends at 23860.
- **_grPixelPipe +180/+184/+188 PINNED** (numScanlines/scanlineInfo/yStart;
  +168/+172/+176 = color/depth/stencil ROW BASES = drawSurface buffers[0/1/2]
  .data + stride*yStart - drawSurface bytes 24/28, 32/36, 40/44 = the three
  _grSurfaceBuffer records after width/height). mutableParams[78] placed at
  +312 (UNPINNED guess until the interpreter session pins cv_anon_1722.m).
- **grPixelPipeline.c (new, Makefile +=)**: grExecutePixelPipeline (DLL
  24757-24775, asserts grPixelPipeline.h 153/154) calls state->
  pixelPipelineCode(pipe) - the hc-compiled program (typedef
  grPixelPipelineCode_fn in grState.h). NULL program = die; tests install a
  noop. The interpreter itself is a later session.
- **grSmall.c += g_mutableParamsMask[12] + grDrawClippedTriangle** (DLL
  26251-26388, asserts grRasterizer.inl 853/854; the __grMemoryCheck stack
  canaries at 906/839 have no hosted equivalent - locals kept alive instead):
  y-sorts the 3 vertex records (stable, record+32 = screenPosition[1]),
  grScanConvertTriangle, on != 0: grSetupTriangleGradients(state, tri,
  unpackIndex(origIndices,0), state->activeGradientMask), per-vertex
  stateMask asserts, row bases, per-active-gradient reference +=
  yAdder*yStart (walks state->activeGradientIndex backwards), grExecute-
  PixelPipeline, then poisons every set bit of g_mutableParamsMask (96
  params?? NO - 78 = GR_MUTABLE_PARAM_COUNT, bytes FE FF FE FF 00 FC FF FF
  FF FF 00 00 = 68 bits set) with -623195394 (0xDAFFFFFE).
- **grrStubs.c: grrDrawClippedTriangle is now a REAL port** (DLL 27381-
  27390): grTestCapability(GR_CAPABILITY_PERSPECTIVE_CORRECTION) ?
  grTriangleTessellator (STUB until the tessellator session) :
  grDrawClippedTriangle(state, tri, origIndices, 8). grTriangleTessellator
  added as silent stub.
- **grState.h**: GR_CAPABILITY_PERSPECTIVE_CORRECTION = 8 (flagged guess;
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST) -> grSetCapability(cap,
  mode==GL_NICEST) DLL 17549-17590) and GR_CAPABILITY_UNUSED = 17 (flagged
  guess; scratch slot grDrawClippedLine clears); GR_VERTEXMASK_COLOR_COMPUTED
  = 0x20 and GR_VERTEXMASK_FORCE_CULL = 0x80 (from the 853/854 masks
  0x2C/0xC0); GR_RENDER_STACK_SCANLINE_INFO 15668; _grRasterPoint/_grEdgePair
  types; state->numActiveGradients/activeGradientIndex[10]/pixelPipelineCode.
- **tests/test_core.c test_scanconvert (+42)**: grDivsu, both scanline
  emitters direct, grScanConvertTriangle hand-worked triangle (v0=(10,12)
  v1=(30,20) v2=(18,28) px, 20.12): plain = 16 rows from y=12 (left/num
  words spot-checked rows 0/1/7/8/9/14/15), x-scissor {13,17} = 13 rows from
  y=13 (front 13|0 + two zero-pixel bottom rows trimmed), y0=16 clip = 12
  rows from y=16 (top half advanced 4 rows, edges advanced, bottom intact);
  end-to-end grDrawClippedTriangle through the y-sort (shuffled records):
  scan state + row pointers + mutableParams poison pattern (params 0/32/40
  NOT poisoned - mask bits clear - params 1/42 poisoned); dispatch test
  (PERSPECTIVE_CORRECTION -> tessellator stub -> no scan state change;
  cleared -> affine path runs); UNUSED cap roundtrip.
- **Hand-arithmetic traps**: 4095*8192>>12 = 8190 EXACT (not 8191); the
  bottom short edge x = 466950 = 28.5004px (not 28.4998) - recompute rather
  than eyeball; CHECK_U/CHECK_P macros do not exist in test_core.c (plain
  CHECK only); mutableParams[i] masks byte i>>3 (param 34 = byte 4 = 0x00).

## DONE session 3q (gradient setup: grSetupGradient/grComputeDeltas/grSetupTriangleGradients)

- **grRasterizer.c (new, Makefile += )**: grSetupGradient (asserts imAssert
  style, grRasterizer.inl 500/501; xAdd from SY deltas, yAdd from SX deltas,
  reference -= hgLsl64h_0_31(sum,20) with sum = full-precision adder products
  x v0sx/v0sy - note hgLsl64h_0_31(sum,20) == sum >> 12), grComputeDeltas
  (asserts 550/551; same plane solve, adders keep full precision for the
  mip-level estimate; the >=32/<32 shift split is VALUE-EQUAL to grSetup-
  Gradient's adders - tested), grSetupTriangleGradients (asserts 603/606/
  628/641/749; imRcpu(abs(area)) low word = shift, high = mantissa;
  signedRecip = hgCopySign32(mant, -area); PerArea = hgMul64h(16*screenDelta,
  signedRecip); texcoord walk texIndex 1..0 x coord 1..0 (T then S),
  gradient = coord + 2*texIndex + 6, per-coord clz normalization
  (clamp 11, <<clamped >>1, preShift = clamped-1, baseValue = shifted v0)
  feeding grSetupTextureUnit; depth via grSetupGradient + optional
  grComputeDepthOffset; fog = final[1][3] (record +54) <<8 base +128;
  colors smooth = final[side][comp]<<8 base +0x8000 (alpha ALWAYS front
  final[0][3]), flat = direct writes reference[xAdder/yAdder] = constant/0
  from the flatShadeIndex record).
- **_grPixelPipe layout pinned** (grState.h): texture units at +0/+64 (64B
  each: +4 pixels, +8 palette, +12 linear, +16 sMask, +20 tMask, +24
  rShift[2], +32 log2DimS - grSetupTextureUnit raw offsets), the 9
  grCheckPixelPipeStack fields moved into a `.checked` block at +128
  (STILL assert-order; grSetupRendering pins), reference[10]@+192,
  xAdder[10]@+232, yAdder[10]@+272 (flat-shade raw offsets), tail to 9216
  total (record = renderStack+14644..23860, pinned by grExecutePixel-
  Pipeline's &renderStack[1830].whichPool). grCheckPixelPipeStack now reads
  pixelPipe->checked.*. GR_GRADIENT_e DEPTH=0/FOG=1/COLOR_R..A=2..5/TEX0_S..=
  6..9 (MAX 10). GR_NUM_RASTERIZER_CACHE_VERTICES stays 28 (clipper temp
  base 92 literal) BUT grSetupTriangleGradients' flatShadeIndex bound is the
  literal 67 (= 64+4-1, rasterizer TU's header scope differs from the
  clipper TU in the original - value wins, string verbatim).
- **grSmall.c += grComputeGradientReference** (DLL 25334-25342, 5-line
  helper) **+ grComputeDepthOffset** (DLL 25902-25940: ref = units +
  reference[0]; switch on drawSurface byte +10 = depthBytes: 1 -> units<<8,
  2 -> units, 4 -> units>>16; factor: += hgLsl64h(factor*max(|xAdd|,|yAdd|),
  16) = factor*max >> 16).
- **grIsDepthOffsetEnabled already existed** (grApiSmall.c, GR_API_CHECK
  148) - duplicate removed from grUtil.c. Caps 12..15 = primitiveType + 11
  (TRIANGLE 3 -> POLYGON_OFFSET_FILL 14).
- **grSetupTextureUnit STUBBED** (grrStubs.c silent): needs the texture-
  object/mipmap layout (tex+4, tex+16*level, tex+176/+180/+183/+204/+206) -
  port with the texture block. Tests keep the tex gradient mask bits
  (0xC0/0x300) clear.
- **hgMul64x32 CARRY BUG FIXED** (hgInt32.c): lowP was declared HGuint32,
  truncating hgMulu64's 64-bit product and losing the partial-product carry
  (decompile DLL 22624 keeps `unsigned __int64 v3`). Only grSetupGradient
  consumes it today but it silently corrupted any 64x32 product whose low
  word overflowed. +8 hg tests (carry cases).
- **imRcpu precision is INHERENT**: mantissa = ~2^62/(area<<clz) with ~2.6e-4
  relative error (8-bit LUT seed + 2 NR steps) - the vertex-fit tolerance in
  tests is +-2048 (fog) / +-0x20000 (colors), while depth stays tight
  (small attribute deltas). The exact-at-v0 identity IS exact: reference
  subtracts the same floor term GRAD_VALUE recomputes.
- **tests/test_core.c test_gradients (+37)**: mask-0 poison, depth exact@
  v0 + plane fits, fog (final[1][3], +128 base), smooth R/G/B/A exact@v0 +
  fit, side=1 (back colors, front alpha), flat constants from
  flatShadeIndex=1 (side 0 and 1, zero adders), depth-offset matrix
  (depthBytes 1/2/4 x units, factor term), grIsDepthOffsetEnabled on/off,
  grSetupGradient-vs-grComputeDeltas adder equality at shift 0 and 20,
  reference identity. Screen records in real format: x/y = pixel<<12 (corrected in 3r: 20.12)
  (IVector4_viewportTransform <<8 pad), area = grRoundTriangleArea
  (area64 >> 23) of grComputeTriangleArea.

## DONE session 3p (grFetchPrimitives + grRenderElements/grRenderArrays + grCheckPixelPipeStack)

- **grApiFast.c additions**: grCheckPixelPipeStack (asserts 491-500: pixelPipe
  color/depth/stencil strides vs drawSurface buffers[0..2].stride (ints
  7/9/11), fogColor/alphaReference/stencilReference/stencilReferenceMasked/
  stencilTestMask/stencilWriteMask vs the packed state), grFetchPrimitives
  (dll 32611-32933; asserts 1298 state/1299 fetchType 0..20/1300 vertexCount
  >= numVertices; sets vertexPipelineDirty=1 FIRST; per-type index walks
  ending in grFlushPrimitiveBatch), grRenderElements (asserts 1606-1619;
  numVertices 1/2/3 by primitive class; TEXTURE_RECT(7) dies at assert 1615;
  fetchType = 3*primitiveType + (UINT16 ? 1 : 0); ends with grCheckPixelPipe-
  Stack), grRenderArrays (asserts 1647/1648; fetchType = 3*primitiveType+2
  computed BEFORE the compilationFailed check; TEXTURE_RECT goes to the fetch
  with numVertices 0 -> fetchType 23 -> dies at assert 1299/1562; NO
  grCheckPixelPipeStack call).
- **GR_DATATYPE_UINT16 = 3 pinned** (grDefs.h, was the GR_DATATYPE_3
  placeholder): oglMapDataType maps GL_UNSIGNED_SHORT 0x1403 -> 3; the index
  datatype enum shares the vertex-array enum names but the array paths never
  accept 3 (their type switches only handle 0/1/2/6/7).
- **Enums pinned** (grState.h): GR_PRIMITIVE_* (POINTS=0, LINES=1, LINE_LOOP=2,
  LINE_STRIP=3, TRIANGLES=4, TRIANGLE_STRIP=5, TRIANGLE_FAN=6, TEXTURE_RECT=7,
  MAX=8) and GR_FETCHTYPE_* (3*primitive + {0=u8,1=u16,2=sequential}; 21
  values, TRIFAN_ARRAY=20, MAX=21).
- **Fetch walk semantics** (all verified by tests): do-while
  (vertexCount >= numVertices) after each consume; POINTS 1/consumed (u8/u16/
  sequential); LINES 2/consumed, qp2(bytes[1], bytes[0]) - swapped;
  LINELOOP 1/consumed strip-pattern + closing segment qp2(first, last) (u8
  cursor +1 byte, u16 +1 short, ARRAY vtxIndex++ with closing qp2(baseIndex,
  vtxIndex)); LINESTRIP same without closing; TRIANGLES 3/consumed with the
  decompile's rotated order qp3(src[2], src[0], src[1]); TRISTRIP 1/consumed
  with stripSwap winding (iter k reads src[k+swap], src[k+1-swap], src[k+2]
  after k advances: tri2 of {0,1,2,3} = (3,2,1) NOT (3,1,0)); TRIFAN center =
  src[0] absolute (bytes[fetchIndex+1], bytes[fetchIndex+2], ARRAY uses
  baseIndex + fetchIndex).
- **grrStubs.c now SILENT returns** (was die-stubs): the queue/flush machinery
  is under test through them; the fetch lists at renderStack+1264 (smooth/
  provoking, 0x20 bit) and +1196 (per-vertex, 0x10 bit) are observable after
  grRenderElements/Arrays - both 0xFF-terminated, built by the BACKWARD
  command walk (reverse command order), dedup via per-vertex-record
  stateMask bits (records keep bits until re-allocated: grVertexCacheGet
  resets stateMask on allocation).
- **State fields added**: compilationFailed (u8, grRenderElements/Arrays
  no-op when set), pixelPipeline (void*), packedFogColor (u32). _grPixelPipe
  struct = cv_anon_1722 with the 9 checked fields in assert order (byte
  offsets unpinned until grSetupRendering/pixel-pipeline session).
- **Test-data traps** (cost several probes): positions must satisfy
  |x|,|y|,|z| <= 1.0 AND within the scissor in SCREEN space (scale 0x640
  maps 1 unit to ~12.8 pixels; y=-0.54 -> negative screen y -> scissor
  reject; x>1.0 -> clip bit -> point AND-reject); collinear positions give
  zero area -> triangles silently dropped (the identity-transform rounding
  makes "exactly collinear" floats land on- or off-line unpredictably -
  use scattered y = 0.07*((i*7)%13)); hash slots for raw 8 = 9 (8^1),
  10/11 -> 11/10, 12/13 -> 13/12, 14 -> 15.
- **tests/test_core.c**: test_fetchprimitives (+53): POINTS array walk
  (slots 11/10/13/12/15, lists [15,12,13,10,11]), TRIANGLES u8 (rotated
  packing, smooth [9,5,2], pipe [9,6,7,5,3,4,2,0,1]), LINELOOP u8 (closing
  segment; lists [0,3,2,1]), TRIFAN u8 (smooth [4,3,2], pipe [4,0,3,2,1]),
  TRISTRIP u16 (tris (2,0,1),(3,2,1); smooth [3,2], pipe [3,2,1,0]),
  LINES u16 (smooth [3,1], pipe [3,2,1,0]), degenerate-triangle drop,
  compilationFailed short-circuit, listlen helper. grSetupCulling must run
  per primitive class before each render (state->primitiveType drives the
  flush re-arm: freeSlots = 64 - grGetPrimitiveVertices).
- **Deferred**: grSetupTextureRectGradients (dll 33077-33172) + grRenderTexture-
  Rect (texture-rect path; needs gradients), grUpdateVertexPipeline already
  ported, pixel pipeline + rasterizer + grrDrawClipped* + data tables.

## DONE session 3o (grQueuePrimitive + grFlushPrimitiveBatch + grClipPrimitive + area helpers)

- **grApiFast.c additions**: grComputeTriangleArea (dll 25943: 64-bit signed
  madd over screenPosition x/y of the three unpacked cache records:
  (x1-x0)*(y2-y0) + (y1-y0)*(x0-x2)), grRoundTriangleArea (00424970: negative
  area -> (a<<9)>>32, positive -> -((-a<<9)>>32)), grQueuePrimitive (dll
  32218-32588) + the grQueuePrimitive1/2/3 degenerate-rejection wrappers
  (32591-32618), grFlushPrimitiveBatch (dll 31970-32124). grClipPrimitive
  (grClipper.inl 559) in grClipper.c. grViewportTransformVertex (dll 28120)
  in grVertexPipeline.c: stateMask |= 8 + IVector4_viewportTransform into
  screenPosition (xyShift 8).
- **grQueuePrimitive flow** (all pinned): previous-primitive cache reuse
  (prevVertexIndices/prevCacheIndices checked BEFORE the hash table; the
  saved prev values are read at entry and the arrays written per vertex),
  aliasing for numVertices<3 (ci1=ci0, ci2=ci1), grSetIndices(ci0,ci1,ci2)
  packed CACHE indices, distinctness asserts 999/1000/1001, flush trigger
  hgSignBit32((254 - queueIndex) | (totalFreeSlots - numVertices)) ->
  grFlushPrimitiveBatch (the queue region is exactly 128 commands:
  renderStack+168..1191, the u32 counter at +1192 counts DWORDS, commands
  written at queue+4*index), AND-reject (clipMask AND of the three records
  & 0xFFC3 - NOTE: a POINT whose vertex has any clip bit is silently
  dropped because all three packed bytes are the same record), OR-clip path
  (& 0xFC3F -> command {packed | 0x10000000, 0}), clip-space stp cull
  (IVector4_stp over the three clipPositions; culled when the sign equals
  cullNegative; runs only for numVertices==3 && CULL_FACE), viewport
  transforms (stateMask bit 8 gate), scissor grouping test (x0/x1/y0/y1
  AND-groups over the used vertices, ORed; sign -> reject), screen-area
  cull ((areaSign ^ cullNegative) & CULL_FACE; queued when ==0 and the area
  is nonzero) -> command {packed | triSide<<24, grRoundTriangleArea} for
  triangles, {packed, 0} for lines/points.
- **CRITICAL ORDERING** (caught by the tests): the viewport transforms run
  BEFORE the subpixel scissor reads (fresh vertices must be transformed or
  the scissor test sees stale positions). My first port read the screen
  positions first - the scissor rejects never fired for fresh vertices.
- **grFlushPrimitiveBatch flow**: grUpdateVertexPipeline (already ported -
  grVertexPipeline.c 1005), backward walk building two fetch lists:
  smooth/provoking list at renderStack+1264 (stateMask bit 0x20; only byte 0
  of each command when SMOOTH_SHADING cap 9 is off, all bytes when on) and
  the per-vertex list at +1196 (bit 0x10, every vertex); both 0xFF-
  terminated; the +1264 list feeds fetchVertexColor/fetchVertexNormal+
  grApplyLighting, the +1196 list feeds fetchTexCoord[0..1]/fetchPointSize/
  applyFog (all NULL-gated); forward dispatch per command: 0x10000000 flag
  -> grClipPrimitive(state, cmd0, grGetPrimitiveVertices(primitiveType)),
  else grrDrawClippedPoint/Line/Triangle by primitiveType (assert 687 for
  non-triangle); final cache re-arm: batchLockMask = setBit of the three
  prevCacheIndices, totalFreeSlots = 64 - grGetPrimitiveVertices, allocIndex
  = 64, queueIndex = 0.
- **grClipPrimitive flow**: grClipper fan -> numVertices==2 forces n=2 ->
  per-fan-vertex scissor recheck (outside-bit mask per vertex via
  hgSignBit32 of the four edge diffs; GR_SCISSORMASK asserts 730-738;
  combinedMask AND; nonzero -> drop) -> triangle path: total fan area ->
  triSide = frontFace ^ totalSign; per fan triangle: (sign ^ cullNegative) &
  CULL_FACE, drawn when != 1 and area nonzero; gradient interpolation loop
  k in [triSide, j+2] over fan vertices with stateMask bit 0x40 ->
  grInterpolateVertexGradients(state, v, origIndices, triSide); assert 813
  triSide in {0,1}; record {grSetIndices(fan0, fan[j+1], fan[j+2]), round-
  Area, triSide} -> grrDrawClippedTriangle; line path: record
  {grSetIndices(fan[0], fan[1], fan[1]), 0, 0} -> grrDrawClippedLine
  (a clipped POINT also lands here - degenerate line == the point).
- **grrStubs.c** (scaffolding, die-stubs until their sessions):
  grInterpolateVertexGradients, grrDrawClippedPoint/Line/Triangle.
  grUpdateVertexPipeline turned out to be already ported - NOT stubbed.
- **GR_CAPABILITY_SMOOTH_SHADING = 9** pinned from the EPOC glShadeModel
  (sub_22018(caps, 9, value); GL_FLAT 7424 clears, GL_SMOOTH 7425 sets).
  GR_SIDE_FRONT 0 / GR_SIDE_BACK 1 + GR_SCISSORMASK_* defined in grState.h.
  _grClipTriangle = {indices, area, side} (cv_anon_17A7).
- **hgMul64h floors toward -inf**: screen y under the y-flip scale -1600 is
  -0x6400, not -0x6300 (floor(-99.99) = -100) - the cull test pins round(-
  648806400) = -78 (0xffffffb2), NOT -77.
- **tests/test_core.c**: test_queueprimitive (+31): point/line/reuse-line
  commands + prev-cache arrays + free slots, degenerate wrapper rejection,
  clipped-point AND-reject, clip-command {packed|0x10000000, 0}, AND-reject
  with shared plane bits 0x44, plain triangle {0x00251c1d, 0x4d}, scissor
  rejects (offset +/-1600000), y-flip cull keep {0x012e2d2c, 0xffffffb2}
  + stp cull drop, flush-trigger boundary (128 commands = 256 dwords, queue
  never crosses; the 129th primitive would flush - untestable until the
  rasterizer lands because the dispatch hits the grr die-stubs).
- **Deferred to the rasterizer session**: grFetchPrimitives (FULLY DECODED:
  21 fetch types = 3*primitiveType + source {0=u8,1=u16,2=sequential};
  POINTS 1/consumed, LINES 2/consumed, LINELOOP strip-pattern (1 index per
  segment, closing segment (first, last)), LINESTRIP strip-pattern, TRIANGLES
  3/consumed, TRISTRIP stripSwap-wound 1/consumed, TRIFAN (v0, k, k+1) from
  absolute srcIndices offsets / baseIndex for ARRAY; vertexPipelineDirty=1;
  asserts 1298/1299 (fetchType 0..GR_FETCHTYPE_MAX-1=20)/1300; ends with
  grFlushPrimitiveBatch), grRenderElements/grRenderArrays (asserts 1606-1619
  /1647-1656; numVertices 1/2/3 by primitive class; TEXTURE_RECT rejected in
  elements (assert 1615) and dies in arrays at assert 1299 (fetchType 23) -
  texture-rect draws go through grRenderTextureRect), grUpdateVertexPipeline
  already done, grrDrawClipped* + grInterpolateVertexGradients + scan
  conversion.

## DONE session 3n (grFast vertex cache: grVertexCacheInvalidate + grVertexCacheGet)

- **_grVertexCache struct pinned** (grState.h, 168 bytes at renderStack+0 -
  grSetupRendering assigns state->vertexCache = state->renderStack):
  batchLockMask HGint64@0 (per-slot lock bits), totalFreeSlots@8 (the
  decompile reaches it as cv_anon_1712.clipPosition[0].e), allocIndex@12,
  prevVertexIndices[3]@16 (last queued primitive's raw vertex indices,
  grQueuePrimitive u32 accesses), prevCacheIndices[3]@28, cacheIndices[32]@40
  (hash table, 4 packed cache-index bytes each). Command queue starts at
  renderStack+168 (42 dwords = 8-byte commands), commandQueueIndex u32@1192
  COUNTS DWORDS (invalidate zeroes it; grQueuePrimitive appends 2 per
  command: {packedIndices|0x10000000, 0} for clipped primitives). Render
  stack total 23860 (grApiFast.inl 381 assert string). GR_RENDER_STACK_SIZE +
  GR_RENDER_STACK_COMMAND_QUEUE_INDEX defined in grState.h.
- **grApiFast.c** (new): grVertexCacheInvalidate (asserts 59/60: renderStack-
  Allocated && renderStack, vertexCache; queue index 0, mask 0, prevVertex
  -1x3, totalFreeSlots/allocIndex 64, prevCacheIndices 0x3, vb[0].index -1,
  hgFill8(cacheIndices, 0, 128)) and grVertexCacheGet (dll.c 32127-32215):
  hash = (v ^ v>>3 ^ v>>5), bucket = &0x1F, slot = &0x3F; 4-way byte scan of
  the packed entry against vertexBuffer[byte].index, hits rotate the match
  to byte0 via hgRor32(entry, 8n); miss takes the low-6 slot or, when the
  lock bit is set, the highest unlocked slot 63-hgClz64(hgNot64(mask))
  (asserts 793/830 "hgInRange32(cIndex, 0, GR_VERTEX_CACHE_SIZE-1)"); new
  entry = (old << 8) | slot (byte3 drops); record gets index + stateMask 0,
  weight fetch (MATRIX_PALETTE+MATRIX_INDEX_ARRAY+WEIGHT_ARRAY caps &&
  vertexArray[3].data && vertexArray[4].data -> grFetchWeightAndIndex into
  weights/weightIndices) then grTransformVertex; after the store:
  totalFreeSlots -= hgGetBit64(oldMask, cIndex) ^ 1 and the lock bit is set.
- **tests/test_core.c**: test_vertextcache (+44): invalidate fields; miss
  slot 5 with the full transform pinned (position IFVector4 e=0x7f818181
  m=0x28000000/0x30000000/0x38000000 for (5,6,7); clip shared-exponent
  e=0x81, m=(0x28000000,0x30000000,0x38000000,0x08000000); stateMask 4 =
  TRANSFORMED); hit keeps free slots; bucket entries 11 (0x0A) / 41 (0x2D,
  bucket 9); eviction 0x41 (slot 11 locked) -> 63, entry 0x00000B3F, mask
  0x8000020000000820; byte-1 hit rotation entry 0x3F00000B; get(0) overwrites
  the invalidated vb[0] sentinel.
- **Probe notes**: grVertexCacheGet tests need grSetupCulling first (populates
  objectToClipTransform; grSetMatrix alone leaves it zeroed -> all-zero clip
  output), a vertex array large enough for the biggest index used, and
  state->vertexCache = state->renderStack (the real layout). grTransform-
  Vertex requires hgInRange32(vertexArray[0].size, 2, 4).

## DONE session 3m (grClipper + vertex-cache/clip helpers + _grVertex layout pin)

- **_grVertex layout CORRECTED** (grState.h) - the old layout was a guess that
  happened to be self-consistent; the clipper's raw offsets prove the real
  one. Pinned from EPOC grTransformVertex (epoc.c 22050-22280) + WINSCW
  grClipper accesses (dll.c 30967+): index@0 (i32), stateMask u16@4 (bits
  0..13 GR_VERTEXMASK_*, 14..15 position state), clipMask u16@6 (written by
  grComputeVertexClipCodes: EPOC (v13+4) & 0x3FFF | 0x4000), clipPosition
  _IVector4@8 (20B; EPOC pool+8/sub_2EB08 = modelview path, +2064 = objectTo-
  Clip projection path, +1864 = matrixPal; guardband codes built at 22257),
  screenPosition[3]@28 (IVector4_viewportTransform output, 3 ints), color@40
  (final[0] u16 @40..46, final[1] @48..54 - EPOC sub_2E270 blends @40+8u),
  texCoord[2]@56/64 (sub_2E270 record+8u+56, barycentric blend >> 29),
  position@72 (fixed LABEL_39 4x i32 / float sub_32BF0), normal@92
  (XNormal3), pointSize@104, weights[4]@108 (sub_315B4 dst v65 = record+108),
  weightIndices@124 (sub_17890). grLightVertex/grApplyLighting/grApplyFog/
  grGetFogZ/grFetchWeightAndIndex/grTransformVertex needed no code change
  (they use field names); grVertexPipeline.c comment on the buffer updated.
- **grClipper.c** (new; grClipper.inl 30654-31488 + grUtil.h helpers):
  grIsInsideClipPlane/Outside/OnPlane (IScalar_testge/testle/testeq),
  grSubpixelTrunc (>>12), grGetFrontFace, grClipFactorSet/Init/Lerp
  (grUtil.h 298-332; cv_anon_170C = int pair), grVertexCacheGetTemp (assert
  40; temps at vertexBuffer[92+i], index=-1, stateMask=0, returns 92+i),
  grClipInterpolate (asserts 121/129; negate-both when d0>=0, t = toUnit-
  Scalar(mul(rcp(d0-d1), d0))), grComputeDistanceToGuardbandPlane (dll
  23728: axis (plane>>1)+2 -> >=3 wraps to (plane>>1)-1; shift 0 for
  plane&6==0 else the caller's 2; c+w / w-c by plane&1; e==254 clamps to
  0x3FFFFFFF via hgCopySign32; zero-normalized-but-nonzero -> sign
  IScalar_set(1, 0x20000000 copySign)), grComputeDistanceToUserClipPlane
  (asserts 164/183; eq copied/negated by planeSign, IScalar dot over the 4
  clipPosition components, negate when planeSign<0), grClipConvexPolygon
  (asserts 259/269; per edge: both-inside or on-plane->outside copies the
  source, crossings copy the inside source first (edgeFlag 0) + the lerp
  temp (swapDirection swaps the pair before grLerpClipFactor); post-pass
  drops reflex vertices via imDet2_64 < 0 over the (a,b) factor pairs),
  grClipper (asserts 396/489/584/645; mask = OR of orig clipMask u16 & 0xFC3F
  (GR_CLIPMASK_CLIPPING_PLANES); per plane: distances for the 3 ORIGINAL
  vertices, barycentric weights (a,b,ONE-a-b) x IVector3_clipperDot, all-
  outside -> 0, sign-change asserts, grClipConvexPolygon + buffer swap
  (user planes swap direction by getFirstNonZeroSign >= 0); fan emit:
  originals keep edgeFlag = cache index (assert 645: mask clean), temps get
  stateMask 69 (0x1|0x4|0x40), index -1, clipMask 0, factors stored to
  normal[0..1] (+92/+96 deliberate alias), clip position = sum of orig
  clipPosition x IUnitScalar_tos(factor & ~0x3F) via IVector4_smul/add,
  normalized by IVector4_fixClipSpacePosition(2)).
- **Constants pinned** (grState.h): GR_CLIPMASK_CLIPPING_PLANES 0xFC3F,
  GR_VERTEX_CACHE_SIZE 64, GR_NUM_RASTERIZER_CACHE_VERTICES 28 (temp base
  92 = EPOC renderStack[1638].whichPool), GR_MAX_CLIPPER_GENERATED_VERTICES
  12, GR_MAX_CLIPPER_VERTICES 15 (decompile stack size).
- **Clip factors alias normal[0..1]**: the clip polygon stores (a,b) in the
  vertex records' normal slots (dll.c 30993: *(int*)(vtx+92) = factor.a,
  +96 = factor.b) - lighting is done by the time the clipper runs. Tests
  pin this (vb[0].normal == (ONE,0) etc. after grClipper).
- **Unit-scalar conversion verified**: IUnitScalar_tos(x) returns an IScalar
  (unit 0.5 -> 0x3F000000); IUnitScalar_lerp(ONE,0,t) with t=0.5-unit gives
  0x10000010 (unit domain, not scalar). grClipInterpolate returns a UNIT
  scalar (t=1/2 == 0x10000000) while grClipFactorSet validates against
  IUNITSCALAR_ONE 0x20000000 = 1.0.
- **tests/test_core.c**: test_clipper (+24): grClipFactorInit slots, grClip-
  Interpolate symmetric t, grVertexCacheGetTemp slots (92/103, index/state-
  Mask), guardband straddle (fan 0,1,92,93; temp factors (0, 0x06666660)
  from lerp(ONE,0,0.8); temp clipPosition e=127 w=0x20000000 z=0xE0000000 =
  w1.0/z-0.5 by hand), all-outside -> 0, factors stored in vb[0]/vb[1].normal.
  IVector4 mantissas are 6.26 IScalar-domain (bit 29 set) - invalid probe
  mantissas die in IVector4_isValid (bsum IMath.c 158), not a clipper bug.
- **Deferred to the rasterizer session**: grClipPrimitive (31235-31488) -
  fully decoded (scissor-mask recheck per fan vertex with GR_SCISSORMASK
  asserts 730-738, stack-magic checks 745/833, triangle fan path: area sum
  via grComputeTriangleArea -> front face = grGetFrontFace ^ hgGet64h>>
  31, per-fan-triangle cull test (sign ^ cullNegative) & CULL_FACE cap,
  nonzero-area gate, grInterpolateVertexGradients on stateMask&0x40
  vertices, assert triSide 813, grRoundTriangleArea, grrDrawClippedTriangle;
  line path: degenerate grSetIndices(i0,i0,i0) + grrDrawClippedLine) - needs
  grrDrawClipped* + gradients. grComputeTriangleArea (dll 25943) + grRound-
  TriangleArea (00424970) decoded: area = madd64(dy01, x2-x0, x1-x0, y2-y0)
  over screenPosition x/y (subpixel), round = neg<<9/h signed variant.

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

## Session 3w — signature machinery (grPixelPipeline.inl 89-988 / DLL 36304-37335)

Committed: signature build + compress + cache glue, all with decompile-matching
assert sites (`grPixelPipeline.inl` file/line strings kept verbatim).

- **grIsGradientUsed** (702; DLL 36949-36959): reads bit `(48+g)>>3` / `(48+g)&7`
  of the HC parameter mask (gradients live at mask bits 48..57).
- **s_mainBitCounts[56] / s_textureBitCounts[44]** (data.fragments.c 3222/3281)
  transcribed verbatim into grPixelPipeline.c. Total significant bits =
  110 + 2x102 = 314 -> exactly 10 packed words = GR_CACHE_SIGNATURE_SIZE 40
  (independently confirms the cache key size).
- **_grSignature (cv_anon_18AA, 144 bytes)** laid out in grState.h:
  descriptor (16B = the drawSurface descriptor ints), then scalars
  isValid@16, logicOp@17(4b), logicOpEnabled@18, blendEnabled@19,
  alphaFunc@20(3b), alphaTest@21, depthFunc@22(3b), depthTest@23,
  depthMask@24, constantDepth@25, colorMask@26(4b), writeColor@27,
  srcFactor@28(4b), dstFactor@29(4b), fogEnabled@30, monochromeFog@31,
  dither@32, depthClampMin@33, stencilFunc/Fail/ZFail/ZPass@34-37(3b),
  stencilTest@38, stencilTestMaskFull@39, stencilWriteMaskFull@40,
  stencilAllOpsEqual@41, stencilZOpsEqual@42, **depthClampMax@43,
  flatShading@44, constantWriteColor@45, perPixelPack@46,
  applyColorWriteMask@47, screenRectGradients@48**, reserved 49..55
  (zero bit counts), texture[2][44]. Byte positions validated by the
  compress asserts (out-of-range values would die) + probe runs with
  single-variable toggles (depthClampMax / flatShading / perPixelPack /
  screenRectGradients each pinned to its byte).
- **grCompressSignature** (845; DLL 36962-37109): per source byte
  srcSize = table[byte], includeBits = min(srcSize, numFreeBits),
  excludeBits = remainder; value = (acc << includeBits) + (src >>
  excludeBits); numFreeBits hits 0 -> flush a word (MSB-first, bytes may
  straddle words); excludeBits != 0 -> residual byte seeds the next
  accumulator. Asserts 845/846/850/860 (main) and 872/881/882/886/896
  (texture loop, src walks &texture[i]) + 903/909 (dst lands exactly on
  &bytes[(40+3)/4] = 10 words).
- **grEvaluatePixelPipeline** (89; DLL 36304-36946): full port. The
  decompile's 0xCC-scratched 8-descriptor combinedMask array is kept
  (value fields), spilled pointer fields replaced by typed locals.
  Behaviour notes pinned by probe: descriptor = drawSurface ints 0..3
  (drawSurface colorMask is descriptor byte 8 — the decompile's
  `*(char *)(&drawSurface + 1)` is byte 8, its element type is 8 bytes);
  buffers[i].data checks are ints 6/8/10; the unit loop counter lives in
  combinedMask[4].colorMask (int-semantics byte, 1 then 0);
  `signature->flatShading` is READ at loop start (always 0 -> the
  constantWriteColor computation is dead in the DLL, transcribed as-is);
  env switch is on the unit record's envMode byte (xAdder[0], unit +8):
  cases 0..4 are the fixed env modes over the texture's PIXELFORMAT
  (0=ALPHA,1=LUMINANCE,2=LUMINANCE_ALPHA,3=INTENSITY,4=RGB,5=RGBA from
  s_infos), case 5 is TEXENV_COMBINE reading the unit record's
  combine sources/ops/scales (record +24..+39), default dies (605);
  post-switch asserts 609-612 (source 0..3, function 0..7);
  rec+40/41 = 1; the "texture replaces vertex color" block (646) clears
  vertexRGB/AlphaUsed and sets rec[23] + perPixelPack = 0.
- **_grTexture raw layout** (cv_anon_1477) pinned where consumed:
  texFormat u16@+8, palette@+176, flags183@+183 (0x10 -> rec17, 0x20 ->
  rasterizer), coordFormat[2]@+204/205 (texcoord data formats 0/1/2 ->
  rec21/22 and the rasterizer rShift switch), flags206@+206 (bit0 -> rec20,
  bit1 -> rasterizer), flags207@+207 (bit0 -> rec19). mipmap base offset
  NOT pinned (named-only access); pads reserve the pinned gaps in
  grTexture.h. grInitTexture/grUpdateTexture keep named access.
- **_grTextureUnitState** (state->textureUnit element, 32B stride
  UNPINNED): texture@0 (xStart[0]), envColor@4 (xStart[1]), envMode@8,
  envFuncRGB@9, envFuncAlpha@10, envSrcRGB[3]@11, envSrcAlpha[3]@14,
  envOpRGB[3]@17, envOpAlpha[3]@20, envScaleRGB@23, envScaleAlpha@24
  (case 5 reads 9..24; xAdder[6..8] = srcAlpha). Units live at elements
  0 and 2 (odd elements = client halves). useTexUnit[2] separate.
- **s_infos[18]** (cv_anon_182B, grrTexture.c): per-format rows
  {colorBytes, rSize, gSize, bSize, aSize, rOffset, gOffset, bOffset,
  aOffset, pixelFormat, paletteFormat} — paletteFormat 0=direct,
  1=RGB565 palette, 2=RGBA4444 palette. **grrGetTextureFormatDescriptor**
  (132/133 asserts) builds a _grSurfaceDescriptor via the grDefs helpers.
- **grMaskedDescriptorsEqual** already existed in grUtilSmall.c (assert
  296); **grIsStencilMaskFull** added to grUtil.c (masks {0,255,65535,0,-1},
  assert 185) + grGetTextureTargetDimensionality dedup (kept in
  grTexture.c only).
- **grSmall.c prefix getters** added (DLL 34729-34780): grGetAlphaFunc /
  SrcBlendFactor / DstBlendFactor / StencilFunc / StencilFail /
  StencilZFail / StencilZPass / LogicOp / RectGradients.
- **Cap chain**: GR_CAPABILITY_DEPTH_OFFSET_POINT = 12, LINE = 13 (=
  grIsDepthOffsetEnabled: primitiveType POINT=1/LINE=2/TRIANGLE=3 + 11);
  POLYGON_OFFSET_FILL = 14 = DEPTH_OFFSET_FILL.
- **grEnsureValidPixelPipeline** (1080; DLL 37112-37279): evaluate ->
  compress -> grCacheFind; hit publishes gradientMask (activeGradientIndex
  walk covers gradients 0..9 ONLY — mask bits 10+ ignored), numActiveGradients,
  activeGradientMask, pixelPipelineCode = entry; stack canary asserts at
  1149/1196. MISS path: the scratch-grow + grExecuteProgram + HC_compile
  loop is transcribed verbatim in a comment and lands with the hc-module
  session (grGetBackend / grScratchGrowMemory / grExecuteProgram /
  HC_compile); until then a miss takes the decompile's compile-failure
  exit (compilationFailed = 1, return).
- **grCheckPixelPipeConsistency** (1213; DLL 37282-37335): re-evaluates +
  re-finds and asserts pixelPipelineCode equality (1235) and gradientMask
  equality (1236) when pixelPipelineDirty == 0; canary assert 1239.
- Tests: test_signature (+105 checks, 945 -> 1050): grIsGradientUsed bits,
  10-word compress pins for a table-driven signature, full
  grEvaluatePixelPipeline state walk (scalar pins + both texture records +
  compress word pins), ensure-valid miss (compilationFailed=1) and hit
  (mask 0x1234 -> indices {2,4,5,9}, bit 12 ignored) + consistency pass.
  295 hg / 311 imath / 1050 core, 0 failures.

## Session 3x — grSetupRendering / grCleanupRendering (grApiFast.inl 288-463)

Committed: per-frame rendering setup/teardown with decompile-matching assert
sites (`grApiFast.inl` file/line strings kept verbatim).

- **grSetupRendering** (288; DLL 31702-31859): flushes clears, locks the draw
  surface (grrBeginRendering → grLockSurface), refreshes per-unit texture
  flags (grUpdateTexture when flags183 bit0 set, pixelPipelineDirty=1;
  useTexUnit[i] = flags183 bit2), validates the pixel pipe
  (grCheckPixelPipeConsistency), rebuilds the render stack when the pipeline
  changed (asserts 340/367/376/381), publishes the pixel-pipe head block
  (dither kernel, buffer strides, fog/alpha/stencil refs at +128..+167),
  validates the scissor chain (asserts 424-432), invalidates the vertex
  cache. The render stack is 23860 bytes allocated from scratchAllocator;
  pixelPipeline = renderStack + 14644 (the decompile's extraStack expression
  is a decompiler artifact — grExecutePixelPipeline passes renderStack+14644
  to the program).
- **grCleanupRendering** (460; DLL 31863-31893): asserts renderStackAllocated
  (460) and scratchMemory/scratchMemorySize (463), frees the render stack,
  releases the draw surface (grrEndRendering → grReleaseSurface).
- **grrBeginRendering / grrEndRendering / grrUpdateState** (via call sites;
  DLL 27362-27378): tiny wrappers around grLockSurface/grReleaseSurface and
  grEnsureValidPixelPipeline.
- **_grPixelPipe head block pinned**: ditherKernel@+128, then the
  grCheckPixelPipeStack assert-order block at +132..+167 (colorBufferStride,
  depthBufferStride, stencilBufferStride, fogColor, alphaReference,
  stencilReference, stencilReferenceMasked, stencilTestMask,
  stencilWriteMask). The 3p placeholder was off by 4 (missing the dither
  kernel slot); grCheckPixelPipeStack updated to match.
- **Gerbera_s.scratchAllocator** added (_grStackAllocator, near
  scratchMemory/scratchMemorySize).
- **hc/egl stubs** (grrStubs.c): grGetBackend (NULL), HC_extraStackSpace (0),
  grScratchGrowMemory (0) — pending the hc module; grLockSurface/
  grReleaseSurface already in grrClear.c.
- Tests: test_setuprendering (+18 checks, 1050 -> 1068): full setup with
  pre-inserted cache program, head-block pins (dither kernel, strides,
  fog/alpha/stencil refs), cleanup frees the stack. 295 hg / 311 imath /
  1068 core, 0 failures.

## Session 3y — grSetupTextureRectGradients (grApiFast.inl 1687)

Committed: texture-rect gradient setup with decompile-matching assert site.

- **grSetupTextureRectGradients** (1687; DLL 33077-33172): per-unit
  texture-coordinate gradients (xAdder/yAdder = (dTx/dTy << 12) / (dFx/dFy)
  via hgSet64s/hgLsl64/hgDiv64_32/hgGet64l, pairs = txMin+(xAdder>>1)-1 /
  tyMin+(yAdder>>1)+1, grSetupTextureUnit), fog gradient (fogAtZero when
  fogMode==0, reference[1] = (fog & 0xFFFF) << 8), flat color gradients
  (reference[i+2] = defaultColorRGBA[i] << 8), depth gradient
  (IScalar lerp zNear..zFar by info->z, IScalar_toFixedHO(...,30)).
  Gradient indices: tex unit i at 2*i+6/2*i+7, fog at 1, colors at 2..5,
  depth at 0. The texture-enable mask is (192 << 2*i) & gradientMask.
- **_grTRGradSetupInfo** defined in grState.h: z@0, fx0/fy0/fx1/fy1@4..16,
  txMin/tyMin/txMax/tyMax pointers@20..32.
- Tests: test_texturerect (+18 checks, 1068 -> 1086): texture adders
  (40960 for 1000<<12/100), fog reference, color references, depth
  reference (IScalar_toFixedHO(0.5f,30) = 0x1FFFFFE0). 295 hg / 311 imath /
  1086 core, 0 failures.

## Session 3z — grRenderTextureRect (grApiFast.inl 1831)

Committed: texture-rect draw with decompile-matching assert sites.

- **grRenderTextureRect** (1831; DLL 33175-33511): clips the IScalar rect to
  the viewport (GR_MAX_VIEWPORT_DIMENSION = 2048, asserts 1925-1953),
  computes per-unit texture coordinates from the crop rect (tex+188..+200
  shifted by 21-log2dim), sets up the pixel-pipe gradients via
  grSetupTextureRectGradients, fills the scanline info ((width<<16)|xStart),
  advances gradient references by yStart, and executes the pipeline.
  Early-outs: zero-size rect, fully off-screen, degenerate after clipping.
- Tests: test_renderrect (+2 checks, 1086 -> 1088): compilationFailed
  early-out. 295 hg / 311 imath / 1088 core, 0 failures.

## Session 4a — hc module foundation (hcOpcode.h/c)

Committed: HC instruction encoding with decompile-matching assert sites.

- **HCInstr_s** (hcOpcode.h): two 32-bit words. instr0: bits 0-7 = opcode,
  bit 8 = hasImm, bits 10-20 = dst, bits 21-31 = src0. instr1: src1 or imm.
- **HC_opcodeDecls[55]** transcribed verbatim from the DLL (docs/libGLES_CM.dll.c:5820).
  Each byte: bits 0-2 = argc, bits 3-4 = type (0=ALUOP, 1=MEMOP, 2=BRANCH),
  bit 5 = commutative, bits 6-7 = dst flags.
- **Opcode enum** (order pinned by table + range checks): 0-22 ALUOP
  (NOP, MOV, MOVN, ADD, SUB, SUBR, AND, OR, XOR, BIC, MULS, MULU, LSL, LSR,
  ASR, ROL, ROR, EOP=16, ASD, MAXS, MINS, MAXU, MINU), 23-38 MEMOP
  (LBS, LSD, SB, SW, SW2, LD, SD, LD4, SD4), 39-54 BRANCH (BRA=39, LABEL=40,
  CBEQ..CBA). EOP at 16 (argc=0), BRA at 39 (argc=0), LABEL at 40 (argc=1).
- **Instruction generation**: HC_genOp0/1/1Imm/2/2Imm/3/3Imm/3Raw/3ImmRaw
  (genOp3Internal/genOp3ImmInternal with range asserts).
- **Opcode accessors**: HC_getOpcodeOper/ArgCountOp/ArgCount/Dst/Src0Raw/
  Src0/Src1Raw/Src1/TypeOp/Flags/HasImm/IsCommutative/Imm/Label/Type,
  HC_isStoreOp (31-38), HC_isLoadOp (23-30), HC_isBranchOp (40-52),
  HC_isCondBranchOp (41-52), HC_isBranch, HC_opcodeDstFlagsOp,
  HC_setOpcodeDst/Src0/Src1/Imm/NoImm.
- Tests: test_hcopcode (+32 checks, 1088 -> 1120): genOp3/3Imm/2/0 round-trips,
  type/store/load/branch range checks, setOpcodeDst. 295 hg / 311 imath /
  1120 core, 0 failures.

## Session 4b - grExecuteProgram bytecode interpreter (grInterpreter.inl)

Committed: the pixel-pipeline bytecode VM, `g_pixelPipeBytecode[3122]`, and
test_interpreter (1120 -> 1325 checks, all green: 295 hg / 311 imath / 1325 core).

- **src/gerbera/hc/grInterpreter.c** - `grExecuteProgram(result, signature,
  resultLength)` interprets `g_pixelPipeBytecode` into HCInstr sequences
  (returns instruction count incl. trailing EOP). Decomp: docs/libGLES_CM.dll.c
  35719-36295.
- **src/gerbera/hc/g_pixelPipeBytecode.c** - the 3122-byte program extracted
  verbatim from docs/libGLES_CM.dll.c:2413.
- **VM layout** (byteImm[190] in the DLL, all one int array): execute stack
  grows UP from index 62 (190 total), repeat stack from 52, return stack from
  36; array memset 0xCC first (decomp `memset(byteImm, 0xCC, 0x2F0)`), then
  dstIndex/curColor/curGradient/curTexture zeroed. stackTop/stackSecond are
  cached from stackPtr[-1]/[-2] BEFORE the control-op pop.
- **Two nested loops**: inner loop reads opcode bytes and emits HC
  instructions (opcode & 0x80 == 0) WITHOUT popping, looping back to the read;
  control instructions (bit 7 set) break out, pop `(opcode>>5)&3` operands,
  check low5==9 (ends program), then switch on low 5 bits (0x00-0x1F).
  Opcode 0x12 = `goto LABEL_10` (continue without consuming a byte).
- **HC decode**: low 6 bits = opcode index (55 -> stackTop as op, 54 -> LABEL
  with 1-byte operand), bit 6 = hasImm; 3-byte dst/src0/src1 with a 3-round
  rotation, register remap per round (254->stackTop, 253->stackSecond,
  128-141 += 14*curTexture, 160/171/182/193 += curGradient+1,
  70/75/80/90/95/100/105/110/115/120 += curColor+1); hasImm reads 1 byte, or
  4 bytes big-endian when src1==0; src1==255 -> imm = stackTop;
  branch ops with dst 11-13 += 3*curTexture.
- **16-bit immediates are BIG-endian** (`p[1] | (p[0]<<8)`): all case 0/1/6/
  0xA/0xB/0xD sites.32-bit immediates also big-endian. (First port used LE and
  ran off the 3122-byte program.)
- **Repeat frame** (case 0xC): pushes [limit=stackTop, counter=0]; case 0xD
  ++counter (repeatPtr[-1]) == limit (repeatPtr[-2]) -> pop else jump;
  cases 0xE/0xF/0x10 read curGradient/curTexture/curColor from the COUNTER
  (repeatPtr[-1]), not the limit.
- **Asserts** in the decomp divide byte differences by 4; with C pointer
  arithmetic the /4 must be dropped (elements vs bytes).
- Case 6 = push program[stackTop + BE16]; case 1 = push-return + conditional
  jump if top == byte; case 0x11 pushes 8 descriptor bytes (order
  3,7,2,6,1,5,0,4); VM ALU cases 0x13-0x1F use the PRE-pop cached operands
  (case 0x14 = top - second).
- **test_interpreter**: minimal signature -> count == 200 (pinned), last op
  EOP, first not EOP, every opcode < HCOP_MAX (per-instruction checks).
  Deterministic at -O0 and -O2 after the 0xCC fill + unsigned-shift fixes
  (uninitialized-stack UB made -O0 diverge into the range assert).
- Next in hc: HC_compile (DLL 51761), HCX86Backend (fragments in
  generated/fragments/hc/), grGetBackend/grScratchGrowMemory/HC_extraStackSpace
  (stubs currently inert in grrStubs.c). ARM backend: not named in either
  decomp (main = x86 HCX86Backend; EPOC decomp is all sub_XXXX) - investigate
  EPOC decomp + hcBackend.h interface next.

## Session 4c - ARM backend identified (EPOC decomp + binary vtable recovery)

Docs updated (docs/analysis.md ARM section; this entry). Findings:

- **The ARM decomp is docs/libgles_cm.dll.epoc.c** (44.5k lines): GNU C++ /
  GCCE build of the EKA2 ARM image `libgles_cm.dll.epoc` (202 KB, "EPOC"
  header); `libgles_cm.dso` is only the ELF32 ARM import lib.953 functions,
 866 decompiled, no assert strings (release build). VA->file: `off = VA - 0x7F64`.
- **The ARM build HAS a native JIT backend** (own source, e.g.
  hcARMBackend.c - the x86 string-derived tree only lists hcX86Backend.c).
  Located via grEnsureValidPixelPipeline sub_20904 -> grGetBackend sub_25484
  -> `off_36F6C` (decomp types it `[2]` - truncated).
- **Full HCBackend_s recovered from the binary** at file 0x2F008 (VA 0x36F6C),
  followed by swap[]="3412/0-.+,)*" like x86 backendDecl:
  canCreateImmediate=sub_245C0 (ARM rotated-imm check), internalStackSpace
  **60** (x86 40), internalStackSpaceAfterInvars=NULL, targetRegisterMapping
 =sub_23D7C -> bytes {0..12,14} (14 hard regs, sp skipped), numHardRegs
  **14** (x86 8), preRegallocFixer=sub_2417C, codegen=sub_24AEC (emits ARM
  words via sub_23F2C/sub_24090 etc., 0xE0000000 AL cond), postRegallocFixer
  **NULL** (compileWrap falls back), emulate=sub_24118 (program(stackPtr)),
  slot9=sub_25480 (4-byte `bx lr` no-op; x86 NULL), isNative=1,
  optimizationMask **NULL** (x86 NULL too).
- ARM compileWrap (sub_2C298) confirmed same pass order as x86
  compileInternal: preRegallocFixer(+20), fixImmediates, registerAllocation,
  slot9/optMask(+44) &1 check, panic vs normal regalloc, postRegallocFixer
  (+28, NULL-checked), codegen(+24,5 args). HC_compile=sub_2C76C ->
  compilePriv sub_2C6E8 (emuDst=0) -> setjmpWrap(sub_2C6B8 -> sub_2C298).
- ARM grExecuteProgram (sub_1F87C) matches the session-4b port semantics
  exactly (BE offsets, inner HC loop without pop, repeat counter read) - the
  port is valid for both builds.
- Next: HC_compile port (shared), hcEmulator, then x86/ARM backend bodies
  (ARM: sub_24AEC region 0x24090-0x24AEC + emit helpers; x86: fragments in
  generated/fragments/hc/).

## Session 4d - hcMemory, hcBackendInfo, CodeView HCOP_e, and full hcX86Backend

- **hcOpcode.h / hcOpcode.c**: complete CodeView `HCOP_e` enum (`0..55`,
  extracted from CodeView LF_ENUM records in `docs/libGLES_CM.dll`), full
  55-byte `HC_opcodeDecls` (`HCOP_EOP=0xD0`, `HCOP_LABEL=0xD1`), `indexSize[10]`
  at VA `0x47F9CC`, `HC_getMemOpIndexSize*`, `HC_getDef`, `HC_getRegUses`,
  `HC_getUses`, `HC_setUse`, and `HC_bsf`.
- **hcMemory.h / hcMemory.c**: double-ended `HCArena_s` allocator (`bottom`
  grows up 4-byte aligned unless unaligned code buffer, `top` grows down),
  `HCMemory_s` callback vtables, and `HCRandom_*` PRNG.
- **hcBackendInfo.h / hcBackendInfo.c**: `HCBackendInfo_s`, `hcChains_s`,
  `HC_getFreeHardRegs`, `DUChains_getDUChain`, `HC_getDUChain`, `HCChain_*`,
  and `HC_getUDChain`.
- **hcX86Backend.c**: complete IA-32 JIT backend (`HCX86Backend_create`,
  `EncodeContext`, all IA-32 instruction emitters, `Imm32Set` constant pool at
  `[ESP+12+4*i]`, `checkLoad` `[ESP+imm]` stack-load fusion, EFLAGS tracking to
  elide `CMP reg, 0`, 2-byte short backward `EB`/`7x` branches, `preRegallocFixer`
  lowering `ANSA`/`FILLB`/`FILLW`/`FILLD` and scaled memops, and
  `postRegallocFixer`). All x86 static tables verified from `docs/libGLES_CM.dll`
  (`0x47D808..0x47D8B0`), catching IDA's truncated `cops__cv_7D868[12]`.
- **test_hcx86backend** (`tests/test_core.c`): +93 byte-exact checks (`core`:
  1418 checks, 0 failures).

## Session 4e - Full hcARMBackend ported & verified from ARM32 binary

- **hcARMBackend.c**: complete 32-bit ARM (`A32`) JIT backend ported from
  `docs/libgles_cm.dll.epoc.c` (`sub_23B4C..sub_25484`) and verified directly
  against raw ARM32 machine words & `.rodata`/`.data` bytes in
  `docs/libgles_cm.dll.epoc` (`file_off = VA - 0x7F64`):
  - All 7 static tables dumped verbatim from image bytes: `armMinMaxCond[4]`
    (`0x36EB8`), `armOpcodeInfo[55]` (`0x36EBC`), `armRegRemap[14]` (`0x36F2A`),
    `armAutoSetFlags[16]` (`0x36F38`), `armMemScaleFix[9]` (`0x36F48`),
    `armMemDesc[13]` (`0x36F51`), `armBranchCond[12]` (`0x36F5E`).
  - Disassembled raw ARM32 instructions at 3 sites where IDA's ARM decompiler
    lost caller-saved registers across `sub_23BB8` or missed a 3rd register
    parameter:
    1. `sub_23BF8` (`VA 0x23D14..0x23D20`): `r0 + 0x02000000` setting bit 25
       (`I=1`, register offset) on word/byte `LDR`/`STR` before adding the
       barrel-shifter immediate field.
    2. `sub_23D84` (`VA 0x23DA8` & caller `0x251E4`): 3rd parameter
       `excludeMask` in `r2`, passed as `1u << rd` for `MUL rd, rd, rd` so the
       allocated/spilled scratch register satisfies ARMv4 `Rd != Rm`.
    3. `sub_23F2C` (`VA 0x2406C`): `+ rm` (`((unsigned char)regsPacked) >> 4`)
       in the immediate-shifted register data-processing path.
  - Full `canCreateImmediate`, `preRegallocFixer` (scaled memop lowering,
    `ROR #0 -> MOV`, 3-op imm & 5-op reg `ROL` lowering, `0xFFF`/`0xFFFF` mask
    synthesis), `HCARMBackend_codegen` (`STMFD`/`LDMFD` prologue/epilogue,
    multi-chunk `MOV/ORR` vs `MVN/BIC` immediate synthesis, barrel-shifter ALU
    consumer fusion with `SUB<->SUBR` swap, CPSR `S`-bit branch fusion eliding
    `CMP`/`TST`, 24-bit `B<cond>` word-offset patching), and `HCARMBackend_create`.
- **test_hcarmbackend** (`tests/test_core.c`): +80 word-exact checks (`hg`:
  295, `imath`: 311, `core`: 1498 checks, 0 failures).
- Next: `hcEmulator.c` (compile-time constant folding/substitution helpers) +
  `HC_compile` core (`hcCompiler` / CFG / SSA / liveness / regalloc) +
  `grrStubs.c` wiring.

