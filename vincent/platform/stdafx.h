#ifndef VINCENT_STDAFX_H
#define VINCENT_STDAFX_H
#define EGL_ON_GP2X 1
#define EGL_NO_COMPILE 1
#define OGLES_API
#if defined(__SYMBIAN32__)
#define GLAPI __attribute__((visibility("default")))
#endif
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <new>
#endif
