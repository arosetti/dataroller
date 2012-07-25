#ifndef _SHARED_H_
#define _SHARED_H_

/* TODO spostare dove servono */
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>   /* va_start va_end */
#include <string.h>            /* memset */
#include <assert.h>

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

#define PACKAGE_NAME "dataroller"
#define PACKAGE_STRING "dataroller 0.1.4"
#define PACKAGE_VERSION "0.1.4"

#define USE_TRUNCATE_BIT_ENCODING 1
#define DEBUG 1

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

#endif
