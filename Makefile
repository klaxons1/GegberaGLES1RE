# Makefile for the Gerbera GLES1 reconstruction.
#
# Phase 1: the hg support library builds and passes its unit tests.
# Later phases add imath, gerbera core, api_opengl, hc and egl as they are
# ported (see PROGRESS.md for the live status).

CC      ?= gcc
AR      ?= ar
CFLAGS  ?= -O2
CFLAGS  += -std=c99 -Wall -Wextra -Wno-unused-parameter -Isrc/include
BUILD   := build

LIB_SRC := \
    src/hg/hgAssert.c \
    src/hg/hgInt32.c \
    src/hg/hgFloat.c \
    src/hg/hgFill.c \
    src/gerbera/grUtil.c \
             src/gerbera/grDefs.c \
             src/gerbera/grSmall.c \
             src/gerbera/grRGBA.c \
             src/gerbera/grUtilSmall.c \
             src/gerbera/grTexture.c \
             src/gerbera/grApiSmall.c \
             src/gerbera/grVertexPipeline.c \
             src/gerbera/grRasterizer.c \
             src/gerbera/grPixelPipeline.c \
             src/gerbera/grTessellator.c \
             src/gerbera/grPipelineCache.c \
             src/gerbera/hc/hcOpcode.c \
             src/gerbera/hc/hcMemory.c \
             src/gerbera/hc/hcBitSet.c \
             src/gerbera/hc/hcAdjMtx.c \
             src/gerbera/hc/hcAdjList.c \
             src/gerbera/hc/hcBucketDefs.c \
             src/gerbera/hc/hcDUChain.c \
             src/gerbera/hc/hcBasicBlock.c \
             src/gerbera/hc/hcQueue.c \
             src/gerbera/hc/hcStack.c \
             src/gerbera/hc/hcEmulator.c \
             src/gerbera/hc/hcBackendInfo.c \
             src/gerbera/hc/hcX86Backend.c \
             src/gerbera/hc/hcARMBackend.c \
             src/gerbera/hc/hcCompiler.c \
             src/gerbera/hc/grInterpreter.c \
             src/gerbera/hc/g_pixelPipeBytecode.c \
src/gerbera/grrTexture.c \
 src/gerbera/grrBlit.c \
 src/gerbera/grTransform.c \
 src/gerbera/grFill.c \
 src/gerbera/grrClear.c \
 src/gerbera/grClipper.c \
 src/gerbera/grApiFast.c \
 src/gerbera/grrStubs.c \
    src/gerbera/api_opengl/oglError.c \
    src/gerbera/api_opengl/oglDescriptor.c \
    src/gerbera/api_opengl/oglMatrixStack.c \
    src/gerbera/api_opengl/oglTexManager.c \
    src/gerbera/api_opengl/oglBufObjManager.c \
    src/gerbera/api_opengl/oglUtil.c \
    src/gerbera/api_opengl/oglContext.c \
    src/gerbera/api_opengl/oglGetter.c \
    src/imath/imAssert.c \
    src/imath/IScalar.c \
    src/imath/IUnitScalar.c \
    src/imath/IMath.c \
    src/imath/imInline.c \
    src/imath/IVector3.c \
    src/imath/IVector4.c \
    src/imath/INormal3.c \
    src/imath/IMatrix3.c \
    src/imath/IMatrix4.c

LIB_OBJ := $(LIB_SRC:%.c=$(BUILD)/%.o)

TEST_SRC := tests/test_hg.c tests/test_imath.c tests/test_core.c

.PHONY: all test clean
all: $(BUILD)/libgerbera.a

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libgerbera.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/test_hg: $(BUILD)/libgerbera.a tests/test_hg.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_hg.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_imath: $(BUILD)/libgerbera.a tests/test_imath.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_imath.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_core: $(BUILD)/libgerbera.a tests/test_core.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_core.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_opengl: $(BUILD)/libgerbera.a tests/test_opengl.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_opengl.c $(BUILD)/libgerbera.a -lm -o $@

test: $(BUILD)/test_hg $(BUILD)/test_imath $(BUILD)/test_core $(BUILD)/test_opengl
	$(BUILD)/test_hg
	$(BUILD)/test_imath
	$(BUILD)/test_core
	$(BUILD)/test_opengl

clean:
	rm -rf $(BUILD)
