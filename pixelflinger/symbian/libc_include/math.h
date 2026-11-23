#ifndef PF_SYMBIAN_MATH_H
#define PF_SYMBIAN_MATH_H

#ifndef M_PI
#define M_PI     3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2   1.57079632679489661923
#endif
#ifndef M_LN2
#define M_LN2    0.69314718055994530942
#endif
#ifndef M_LOG2E
#define M_LOG2E  1.44269504088896340736
#endif

#define HUGE_VALF (__builtin_huge_valf())
#define INFINITY  (__builtin_inff())
#define NAN       (__builtin_nanf(""))
#define isnan(x)  __builtin_isnan(x)
#define isinf(x)  __builtin_isinf(x)

#ifdef __cplusplus
extern "C" {
#endif

static inline float fabsf(float x)
{
    return __builtin_fabsf(x);
}

static inline double fabs(double x)
{
    return __builtin_fabs(x);
}

float floorf(float x);
float ceilf(float x);
float sqrtf(float x);

#ifdef __cplusplus
}
#endif

#endif
