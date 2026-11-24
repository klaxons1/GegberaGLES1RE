#ifndef GERBERA_TEST_SUPPORT_H
#define GERBERA_TEST_SUPPORT_H

#include <stdio.h>

static int checks, failures;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

#define CHECK_I64_EQ(expression, expected) do { \
    long long actual_ = (long long)(expression); \
    long long expected_ = (long long)(expected); \
    ++checks; \
    if (actual_ != expected_) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s == %lld, got %lld\n", \
                __FILE__, __LINE__, #expression, expected_, actual_); \
    } \
} while (0)

#define CHECK_U32_EQ(expression, expected) do { \
    unsigned int actual_ = (unsigned int)(expression); \
    unsigned int expected_ = (unsigned int)(expected); \
    ++checks; \
    if (actual_ != expected_) { \
        ++failures; \
        fprintf(stderr, "FAIL %s:%d: %s == 0x%08x, got 0x%08x\n", \
                __FILE__, __LINE__, #expression, expected_, actual_); \
    } \
} while (0)

#endif
