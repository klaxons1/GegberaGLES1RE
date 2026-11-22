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

$(BUILD)/test_hg: $(filter src/hg/%,$(LIB_OBJ)) tests/test_hg.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(filter src/hg/%,$(LIB_SRC)) tests/test_hg.c -lm -o $@

$(BUILD)/test_imath: $(LIB_OBJ) tests/test_imath.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIB_SRC) tests/test_imath.c -lm -o $@

$(BUILD)/test_core: $(LIB_OBJ) tests/test_core.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIB_SRC) tests/test_core.c -lm -o $@

test: $(BUILD)/test_hg $(BUILD)/test_imath $(BUILD)/test_core
	$(BUILD)/test_hg
	$(BUILD)/test_imath
	$(BUILD)/test_core

clean:
	rm -rf $(BUILD)
