#include "bitio.h"
#include "shared.h"

#ifdef __linux__               /* endian conversions */
#ifndef _BSD_SOURCE
    #define _BSD_SOURCE
#endif
  #include <endian.h>
#else 
  #include <sys/endian.h>
#endif

/* buffer size in bytes (must be 8 multiple) */
#define BUFFER_BYTE_SIZE 8192 /* 2^13 */

#define BLOCK_BIT_SIZE_SHIFT_MOD  63

/* block number of the buffer */
#define N_BLOCKS (BUFFER_BYTE_SIZE / 8)

#define HTOLE htole64
#define LETOH le64toh

static uint64_t lmask[65] = 
{
     0UL,
     0x1UL, 0x3UL, 0x7UL, 0xfUL,
     0x1fUL, 0x3fUL, 0x7fUL, 0xffUL,
     0x1ffUL, 0x3ffUL, 0x7ffUL, 0xfffUL,
     0x1fffUL, 0x3fffUL, 0x7fffUL, 0xffffUL,
     0x1ffffUL, 0x3ffffUL, 0x7ffffUL, 0xfffffUL,
     0x1fffffUL, 0x3fffffUL, 0x7fffffUL, 0xffffffUL,
     0x1ffffffUL, 0x3ffffffUL, 0x7ffffffUL, 0xfffffffUL,
     0x1fffffffUL, 0x3fffffffUL, 0x7fffffffUL, 0xffffffffUL,
     0x1ffffffffUL, 0x3ffffffffUL, 0x7ffffffffUL, 0xfffffffffUL,
     0x1fffffffffUL, 0x3fffffffffUL, 0x7fffffffffUL, 0xffffffffffUL,
     0x1ffffffffffUL, 0x3ffffffffffUL, 0x7ffffffffffUL, 0xfffffffffffUL,
     0x1fffffffffffUL, 0x3fffffffffffUL, 0x7fffffffffffUL, 0xffffffffffffUL,
     0x1ffffffffffffUL, 0x3ffffffffffffUL, 0x7ffffffffffffUL, 0xfffffffffffffUL,
     0x1fffffffffffffUL, 0x3fffffffffffffUL, 0x7fffffffffffffUL, 0xffffffffffffffUL,
     0x1ffffffffffffffUL, 0x3ffffffffffffffUL, 0x7ffffffffffffffUL, 0xfffffffffffffffUL,
     0x1fffffffffffffffUL, 0x3fffffffffffffffUL, 0x7fffffffffffffffUL, 0xffffffffffffffffUL
};

static uint64_t hmask[65] = 
{
    0UL,
    0x8000000000000000UL, 0xc000000000000000UL, 0xe000000000000000UL, 0xf000000000000000UL,
    0xf800000000000000UL, 0xfc00000000000000UL, 0xfe00000000000000UL, 0xff00000000000000UL,
    0xff80000000000000UL, 0xffc0000000000000UL, 0xffe0000000000000UL, 0xfff0000000000000UL,
    0xfff8000000000000UL, 0xfffc000000000000UL, 0xfffe000000000000UL, 0xffff000000000000UL,
    0xffff800000000000UL, 0xffffc00000000000UL, 0xffffe00000000000UL, 0xfffff00000000000UL,
    0xfffff80000000000UL, 0xfffffc0000000000UL, 0xfffffe0000000000UL, 0xffffff0000000000UL,
    0xffffff8000000000UL, 0xffffffc000000000UL, 0xffffffe000000000UL, 0xfffffff000000000UL,
    0xfffffff800000000UL, 0xfffffffc00000000UL, 0xfffffffe00000000UL, 0xffffffff00000000UL,
    0xffffffff80000000UL, 0xffffffffc0000000UL, 0xffffffffe0000000UL, 0xfffffffff0000000UL,
    0xfffffffff8000000UL, 0xfffffffffc000000UL, 0xfffffffffe000000UL, 0xffffffffff000000UL,
    0xffffffffff800000UL, 0xffffffffffc00000UL, 0xffffffffffe00000UL, 0xfffffffffff00000UL,
    0xfffffffffff80000UL, 0xfffffffffffc0000UL, 0xfffffffffffe0000UL, 0xffffffffffff0000UL,
    0xffffffffffff8000UL, 0xffffffffffffc000UL, 0xffffffffffffe000UL, 0xfffffffffffff000UL,
    0xfffffffffffff800UL, 0xfffffffffffffc00UL, 0xfffffffffffffe00UL, 0xffffffffffffff00UL,
    0xffffffffffffff80UL, 0xffffffffffffffc0UL, 0xffffffffffffffe0UL, 0xfffffffffffffff0UL,
    0xfffffffffffffff8UL, 0xfffffffffffffffcUL, 0xfffffffffffffffeUL, 0xffffffffffffffffUL     
};

struct bitio
{
    int fd;
    mode_t mode;
    uint32_t pos;
    int len;
    uint64_t buf[N_BLOCKS];
};

void safe_close(int fd)
{
    while(close(fd) < 0)
    {
        if (errno == EINTR)
            continue;

        perror("write()");
        exit(1);
    }
}

size_t safe_read(int fd, uint8_t *buf, size_t count) /* count is in byte, therefore buf is uint8_t* */
{
    size_t done = 0, ret;

    while (done != count)
    {
        if ((ret = read(fd, buf, count-done)) >= 0) /* SSIZE_MAX ?*/
        {
            if (!ret)
                break;

            done += ret;
            buf  += ret;
            continue;
        }

        perror("read()");
        if (errno == EINTR)
            continue;

        exit(1);
    }

    return done;
}

void safe_write(int fd, const uint8_t *buf, size_t count) /* count is in byte, therefore buf is uint8_t* */
{
    size_t done = 0, ret;

    while (done != count)
    {
        if ((ret = write(fd, buf, count-done)) > 0)
        {
            done += ret;
            buf  += ret;
            continue;
        }

        perror("write()");
        if (errno == EINTR)
            continue;

       exit(1);
    }
}

struct bitio *bitio_open(const char *filename, mode_t mode)
{
    int fd;
    struct bitio *p;

    assert(filename);

    if (mode != O_RDONLY && mode != O_WRONLY)
    {
        errno = EINVAL;
        return NULL;
    }

    if ((fd = open(filename,
                   mode==(mode_t)O_RDONLY?O_RDONLY:(O_WRONLY | O_CREAT | O_TRUNC),
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
        return (void*) NULL;

    if (!(p = calloc(1, sizeof(struct bitio))))
    {
        safe_close(fd);
        errno = ENOMEM;
        return NULL;
    }

    p->fd = fd;
    p->mode = mode;
    return p;
}

int bitio_close(struct bitio *p)
{
    uint32_t res;

    assert(p);
    
    res = p->pos/64 + ((p->pos & BLOCK_BIT_SIZE_SHIFT_MOD)?1:0);
    if (res && p->mode != O_RDONLY)
    {
        for (int i = 0; i < res; i++)
            p->buf[i] = HTOLE(p->buf[i]);
        safe_write(p->fd, (uint8_t*)p->buf, res*8); /* flushing buffer */
    }

    safe_close(p->fd);
    memset(p, 0, sizeof(struct bitio));
    free(p);

    return 0;
}

static inline void bitio_check_flush(struct bitio *p)
{
    if (p->pos == N_BLOCKS*64)
    {
        for (uint32_t i = 0; i < N_BLOCKS; i++)
            p->buf[i] = HTOLE(p->buf[i]);
        safe_write(p->fd, (uint8_t*)p->buf, N_BLOCKS*8);
        p->pos = 0;
    }
}

int bitio_read(struct bitio *p, uint64_t *data, uint8_t len)
{
    uint8_t res, k;
    uint32_t ofs;

    assert(p && data);
    assert(0 < len && len < 65);

    *data &= 0;

    while (len > 0)
    {
        if (p->pos == p->len*8)
        {
            p->len = safe_read(p->fd, (uint8_t*)p->buf, N_BLOCKS*8);
            if (!p->len) // end of file
                return 1;

            if (p->len % 8) /* p->len must be multiple of block */
                return -1;
            for (uint32_t i = 0; i < p->len/8; i++)
                p->buf[i] = LETOH(p->buf[i]);
            p->pos = 0;
        }

        ofs = p->pos / 64;
        k = (uint8_t)(p->pos & BLOCK_BIT_SIZE_SHIFT_MOD);
        res = 64 - k;     

        if (len >= res)
        {
            len -= res;
            *data |= (p->buf[ofs] & lmask[res]) << len;
            p->pos += (uint32_t)res;
        }
        else
        {
            res -= len;
            *data |= (p->buf[ofs] >> res) & lmask[len];
            p->pos += (uint32_t)len;
            return 0;
        }
    }

    return 0;
}

int bitio_write(struct bitio *p, uint64_t data, uint8_t len)
{
    uint8_t res, k;
    uint32_t ofs;

    assert(p);
    assert(0 < len && len < 65);

    while (len > 0)
    {
        ofs = p->pos / 64;
        k = (uint8_t)(p->pos & BLOCK_BIT_SIZE_SHIFT_MOD);
        res = 64 - k;
        p->buf[ofs] &= hmask[k];
        if (len >= res)
        {
            len -= res;
            p->buf[ofs] |= (data >> len) & lmask[res];
            p->pos += (uint32_t)res;
        }
        else
        {
            res -= len;
            p->buf[ofs] |= (data & lmask[len]) << res;
            p->pos += (uint32_t)len;
            bitio_check_flush(p);
            return 0;
        }
        bitio_check_flush(p);
    }
    return 0;
}

int bitio_write1(struct bitio *p)
{
    uint8_t res, k;
    uint32_t ofs;

    assert(p);
    
    ofs = p->pos / 64;
    k = (uint8_t)(p->pos & BLOCK_BIT_SIZE_SHIFT_MOD);
    res = 64 - k;
    p->buf[ofs] &= hmask[k];
    p->buf[ofs] |= (uint64_t)1 << (--res);
    p->pos++;

    bitio_check_flush(p);  
    return 0;
}

int bitio_write0(struct bitio *p)
{
    uint8_t k;
    uint32_t ofs;

    assert(p);

    ofs = p->pos / 64;
    k = (uint8_t)(p->pos & BLOCK_BIT_SIZE_SHIFT_MOD);
    p->buf[ofs] &= hmask[k];
    p->pos++;

    bitio_check_flush(p);
    return 0;
}
