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
    src/hg/hgFill.c

LIB_OBJ := $(LIB_SRC:%.c=$(BUILD)/%.o)

TEST_SRC := tests/test_hg.c

.PHONY: all test clean
all: $(BUILD)/libhg.a

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libhg.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

$(BUILD)/test_hg: $(LIB_OBJ) $(TEST_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIB_SRC) $(TEST_SRC) -lm -o $@

test: $(BUILD)/test_hg
	$(BUILD)/test_hg

clean:
	rm -rf $(BUILD)
