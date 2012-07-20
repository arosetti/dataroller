#ifndef _SHARED_H_
#define _SHARED_H_

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>   /* va_start va_end */
#include <sys/mman.h> /* mlockall */

#ifdef USE_INLINE
    #if defined(_MSC_VER)
        #define FORCE_INLINE    __forceinline
    #else
        #define FORCE_INLINE __attribute__((always_inline))
    #endif
#else
    #define FORCE_INLINE
#endif

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#ifndef __STDC_FORMAT_MACROS
  #define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include "../config.h"

#if defined(__GNUC__) && defined(USE_LIKELY)
    #define likely(x)       __builtin_expect((x), 1)
    #define unlikely(x)     __builtin_expect((x), 0)
#else
    #define likely(x)       x
    #define unlikely(x)     x
#endif

#define max(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
 _a > _b ? _a : _b; })

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
   __typeof__ (b) _b = (b); \
 _a < _b ? _a : _b; })

void *my_malloc(size_t);
void *my_calloc(size_t, size_t);

bool info(const char *fmt, ...);

#endif
