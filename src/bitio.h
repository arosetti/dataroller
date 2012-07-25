#ifndef _BITIO_H_
#define _BITIO_H_

#include <fcntl.h>             /* mode_t */
#include <stdint.h>            /* {u,}int{8,16,32,64}_t */

/* opaque type used for stream buffering */
struct bitio;

/* open stream buffer to the given file */
struct bitio*  bitio_open(const char *filename, mode_t mode);

/* close stream buffer flushing the buffer */
int     bitio_close(struct bitio *p);

/* read len bits from the buffer and write them on data */
int     bitio_read(struct bitio *p, uint64_t *data, uint8_t len);

/* write len bits from data and write them on the buffer */
int     bitio_write(struct bitio *p, uint64_t data, uint8_t len);

/* write one bit to buffer, (simpler implementation) */
int     bitio_write1(struct bitio *p);

/* write zero bit to buffer, (simpler implementation) */
int     bitio_write0(struct bitio *p);

#endif /* _BITIO_H_ */
