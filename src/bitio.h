#ifndef _BITIO_H_
#define _BITIO_H_

#include <fcntl.h>             /* mode_t */
#include <string.h>            /* memset */
#include <stdint.h>            /* {u,}int{8,16,32,64}_t */
#include <sys/stat.h>

#ifndef __STDC_FORMAT_MACROS
    #define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

#ifdef __linux__               /* endian conversions */
  #ifndef _BSD_SOURCE
    #define _BSD_SOURCE
  #endif
  #include <endian.h>
#else 
  #include <sys/endian.h>
#endif

#include "shared.h"

/* buffer size in bytes (must be BLOCK_BYTE_SIZE multiple) */
#define BUFFER_BYTE_SIZE      8192 /* 2^13 */

#define BUFFER_BYTE_SIZE_SHIFT_MOD  8191

/* block size in bytes, valid values are: 8 4 2 1 */
#define BLOCK_BYTE_SIZE       8

/* block size in bit, values are: 64 32 16 8 */
#define BLOCK_BIT_SIZE        (BLOCK_BYTE_SIZE << 3)

#define BLOCK_BIT_SIZE_SHIFT_MOD  63

/* block number of the buffer */
#define N_BLOCKS              (BUFFER_BYTE_SIZE / BLOCK_BYTE_SIZE)

#if BLOCK_BYTE_SIZE == 8
    #define BLOCK_TYPE uint64_t
    #define HTOLE htole64
    #define LETOH le64toh
#elif BLOCK_BYTE_SIZE == 4
    #define BLOCK_TYPE uint32_t
    #define HTOLE htole32
    #define LETOH le32toh
#elif BLOCK_BYTE_SIZE == 2
    #define BLOCK_TYPE uint16_t
    #define HTOLE htole16
    #define LETOH le16toh
#else /* elif BLOCK_BYTE_SIZE == 1 */
    #define BLOCK_TYPE uint8_t
    #define HTOLE
    #define LETOH
#endif

/* opaque type used for stream buffering */
typedef struct bitio bitio;

/* open stream buffer to the given file */
bitio*  bitio_open(const char *filename, mode_t mode);

/* close stream buffer flushing the buffer */
int     bitio_close(bitio *p);

/* read len bits from the buffer and write them on data */
int     bitio_read(bitio *p, uint64_t *data, uint8_t len);

/* write len bits from data and write them on the buffer */
int     bitio_write(bitio *p, uint64_t data, uint8_t len);

/* write one bit to buffer, (simpler implementation) */
int     bitio_write1(bitio *p);

/* write zero bit to buffer, (simpler implementation) */
int     bitio_write0(bitio *p);

#endif /* _BITIO_H_ */
