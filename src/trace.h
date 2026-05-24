#pragma once
#include <stdio.h>
#include <time.h>

static inline double trace_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

#ifdef FIB_TRACE
#  define TRACE(...)  fprintf(stderr, __VA_ARGS__)
#  define TRACE_T(t)  (trace_now() - (t))
#else
#  define TRACE(...)  ((void)0)
#  define TRACE_T(t)  ((void)(t), 0.0)
#endif
