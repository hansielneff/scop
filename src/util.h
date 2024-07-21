#ifndef SCOP_UTIL_H
#define SCOP_UTIL_H

// TYPES

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <limits.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef intmax_t isize;
typedef intptr_t iptr;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintmax_t usize;
typedef uintptr_t uptr;

typedef float f32;
typedef double f64;

typedef const char* cstr;

// MACROS

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#ifdef NDEBUG
    #define DEBUG 0
#else
    #define DEBUG 1
#endif

// https://stackoverflow.com/a/1598827
#define ARR_LEN(x) \
    ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

#define ASSERT(x) assert(x)

// https://stackoverflow.com/a/1644898
#define INFORM(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)

// FUNCTIONS

#include <stdlib.h>

static inline void* mallocOrDie(usize size)
{
    void* allocation = malloc(size);
    if (allocation == NULL && size > 0)
    {
        INFORM("%s\n", "Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    return allocation;
}

#endif
