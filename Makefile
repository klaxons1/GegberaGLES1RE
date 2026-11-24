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
 src/gerbera/grrRasterizer.c \
    src/gerbera/api_opengl/oglError.c \
    src/gerbera/api_opengl/oglDescriptor.c \
    src/gerbera/api_opengl/oglMatrixStack.c \
    src/gerbera/api_opengl/oglTexManager.c \
    src/gerbera/api_opengl/oglBufObjManager.c \
    src/gerbera/api_opengl/oglUtil.c \
    src/gerbera/api_opengl/oglContext.c \
    src/gerbera/api_opengl/oglGetter.c \
    src/gerbera/api_opengl/oglFast.c \
    src/gerbera/api_opengl/oglApi.c \
    src/gerbera/egl/eglUtil.c \
    src/gerbera/egl/eglBase.c \
    src/gerbera/egl/eglApi.c \
    src/gerbera/egl/eglSymbian.c \
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

TEST_SRC := tests/test_hg.c tests/test_imath.c tests/test_geometry.c tests/test_core.c tests/test_opengl.c tests/test_egl.c tests/test_symbian_surface.c

ifeq ($(shell uname -s),Darwin)
TEST_GC_LDFLAGS := -Wl,-dead_strip
else
TEST_GC_LDFLAGS := -Wl,--gc-sections
endif

.PHONY: all test check differential differential-build differential-coverage win32 symbian clean
all: $(BUILD)/libgerbera.a

win32:
	./demo/build_win32.sh

symbian:
	./symbian/build_symbian.sh

$(BUILD)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/libgerbera.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/test_hg: $(BUILD)/libgerbera.a tests/test_hg.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_hg.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_imath: $(BUILD)/libgerbera.a tests/test_imath.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_imath.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_geometry: $(BUILD)/libgerbera.a tests/test_geometry.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_geometry.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_core: $(BUILD)/libgerbera.a tests/test_core.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_core.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_opengl: $(BUILD)/libgerbera.a tests/test_opengl.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_opengl.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_egl: $(BUILD)/libgerbera.a tests/test_egl.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) tests/test_egl.c $(BUILD)/libgerbera.a -lm -o $@

$(BUILD)/test_symbian_surface: tests/test_symbian_surface.c src/gerbera/egl/eglSymbian.c src/gerbera/egl/eglUtil.c src/hg/hgInt32.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DNDEBUG -ffunction-sections -fdata-sections -Wno-attributes -idirafter symbian/include tests/test_symbian_surface.c src/hg/hgInt32.c $(TEST_GC_LDFLAGS) -o $@

test: $(BUILD)/test_hg $(BUILD)/test_imath $(BUILD)/test_geometry $(BUILD)/test_core $(BUILD)/test_opengl $(BUILD)/test_egl $(BUILD)/test_symbian_surface
	$(BUILD)/test_hg
	$(BUILD)/test_imath
	$(BUILD)/test_geometry
	$(BUILD)/test_core
	$(BUILD)/test_opengl
	$(BUILD)/test_egl
	$(BUILD)/test_symbian_surface

# Optional original/source checks require Python with unicorn, capstone and pefile.
PYTHON ?= python3
DIFF_ARGS ?=
DIFF_ADAPTERS := tests/differential_raster_adapter.c tests/differential_gl_adapter.c tests/differential_egl_adapter.c tests/differential_guard.c tests/differential_arm_coverage.c tests/differential_clipper_adapter.c tests/differential_texture_adapter.c tests/differential_vertex_adapter.c
DIFF_OBJ := $(LIB_SRC:%.c=$(BUILD)/differential/%.o) $(DIFF_ADAPTERS:%.c=$(BUILD)/differential/%.o)

$(BUILD)/differential/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

$(BUILD)/differential/libgerbera_diff.so: $(DIFF_OBJ)
	$(CC) -shared $(CFLAGS) $^ -lm -o $@

differential-build: $(BUILD)/differential/libgerbera_diff.so

differential: differential-build
	$(PYTHON) tests/differential.py --library $(BUILD)/differential/libgerbera_diff.so $(DIFF_ARGS)

check: test differential

# Optional recovered-C coverage, measured during the same binary comparisons.
COVERAGE_CC ?= clang
LLVM_PROFDATA ?= $(shell command -v llvm-profdata 2>/dev/null || xcrun --find llvm-profdata 2>/dev/null)
LLVM_COV ?= $(shell command -v llvm-cov 2>/dev/null || xcrun --find llvm-cov 2>/dev/null)
COVERAGE_BUILD = $(BUILD)/differential-coverage
COVERAGE_LIB = $(COVERAGE_BUILD)/differential/libgerbera_diff.so

differential-coverage:
	$(MAKE) BUILD=$(COVERAGE_BUILD) CC=$(COVERAGE_CC) CFLAGS='$(CFLAGS) -fprofile-instr-generate -fcoverage-mapping' differential-build
	LLVM_PROFILE_FILE=$(COVERAGE_BUILD)/differential.profraw $(PYTHON) tests/differential.py --library $(COVERAGE_LIB) $(DIFF_ARGS)
	$(LLVM_PROFDATA) merge -sparse $(COVERAGE_BUILD)/differential.profraw -o $(COVERAGE_BUILD)/differential.profdata
	$(LLVM_COV) report $(COVERAGE_LIB) -instr-profile=$(COVERAGE_BUILD)/differential.profdata $(LIB_SRC)

-include $(LIB_OBJ:.o=.d) $(DIFF_OBJ:.o=.d)

clean:
	rm -rf $(BUILD)
