#include "compress_lzw.h"

#define CODE_MIN_MAX_BITS  12
#define CODE_MAX_MAX_BITS  26

#define LZW_CODE_EMPTY    256
#define LZW_CODE_EOF      257
#define LZW_CODE_START    258

/* prime number bigger than ctx->code_max      */
/* http://primes.utm.edu/lists/small/millions/ */
static const uint32_t hash_sizes[15] = 
{
    5021,         /* CODE_MAX_BITS == 12 */
    9859,         /* CODE_MAX_BITS == 13 */
    18041,        /* CODE_MAX_BITS == 14 */
    35023,        /* CODE_MAX_BITS == 15 */
    69001,        /* CODE_MAX_BITS == 16 */
    169937,       /* CODE_MAX_BITS == 17 */
    290047,       /* CODE_MAX_BITS == 18 */
    744811,       /* 691111,   CODE_MAX_BITS == 19 min: 524287*/
    1884119,      /* 1299827,  CODE_MAX_BITS == 20 min: 1048575*/
    2904887,      /* 2206937,  CODE_MAX_BITS == 21 min: 2097151*/
    5794307,      /* 4466963,  CODE_MAX_BITS == 22 min: 4194303*/
    8869187,      /* CODE_MAX_BITS == 23 min: 8388607*/
    17167081,     /* CODE_MAX_BITS == 24 min: 16777215*/
    35086279,     /* CODE_MAX_BITS == 25 min: 33554431*/
    68219119      /* CODE_MAX_BITS == 26 min: 67108863*/
};

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

#define READ_BLOCK_SIZE  8192

#define HEADER_MAGIC     0x00575a4c /* ZWL */

typedef struct lzw_context_enc
{
    /* si fanno separate per l'allineamento */
    uint32_t* table_code;    /* child */ /* FIXME child e parent insieme per < località */
    uint32_t* table_parent;  /* parent */
    uint8_t*  table_symbol;

    #ifdef USE_TRIE
    uint32_t* table_left;
    uint32_t* table_right;
    #endif

    uint8_t  code_max_bits, hash_shift;
    uint32_t code_max, hash_size, table_max;

    bitio *b_dst;
    FILE  *f_src;
    uint32_t  current_parent_code;
    uint32_t  current_max_code;
    uint32_t  new_code;
    uint8_t   new_symbol;
    uint8_t   current_code_bits;
    uint8_t   binary_file;
    #ifdef HASH_STATISTICS
    uint32_t hashing;
    uint32_t tot_collisions;
    uint32_t collisions[21]; /* TODO init to 0 */
    #endif
} lzw_context_enc;

/********* TRIE ********/

#ifdef USE_TRIE
void trie_free(lzw_context_enc *ctx)
{
    if (ctx->table_code)
        free(ctx->table_code);
    if (ctx->table_parent)
        free(ctx->table_parent);
    if (ctx->table_symbol)
        free(ctx->table_symbol);
    if (ctx->table_left)
        free(ctx->table_left);
    if (ctx->table_right)
        free(ctx->table_right);
}

bool trie_init(lzw_context_enc *ctx)
{
    ctx->hash_size = hash_sizes[ctx->code_max_bits - CODE_MIN_MAX_BITS];
    ctx->hash_shift = ctx->code_max_bits - 8;

    if (!(ctx->table_code = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_trie_enc;
    if (!(ctx->table_parent = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_trie_enc;
    if (!(ctx->table_symbol = calloc(1, sizeof(uint8_t) * ctx->hash_size)))
        goto abort_new_trie_enc;
    if (!(ctx->table_left = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_trie_enc;
    if (!(ctx->table_right = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_trie_enc;

    return true;

    abort_new_trie_enc:
    trie_free(ctx);

    return false;
}

void trie_reset(lzw_context_enc *ctx)
{
    assert(ctx);

    for (int i = 0; i < ctx->hash_size; i++)
    {
        ctx->table_code[i] = LZW_CODE_EMPTY;
        ctx->table_left[i] = LZW_CODE_EMPTY;
        ctx->table_right[i] = LZW_CODE_EMPTY;
    }
}

int trie_lookup(lzw_context_enc *ctx)
{
    uint32_t code = ctx->table_code[ctx->current_parent_code];

    while (code != LZW_CODE_EMPTY) 
    {
        if (ctx->new_symbol == ctx->table_symbol[code]) 
        {
            ctx->current_parent_code = code;
            return 1;
        }
        else if (ctx->new_symbol > ctx->table_symbol[code])
            code = ctx->table_right[code];
        else 
            code = ctx->table_left[code];
    }

    return 0;
}

int trie_insert(lzw_context_enc *ctx)
{
    uint32_t code = ctx->table_code[ctx->current_parent_code];

    if (code != LZW_CODE_EMPTY) 
    {
        while (1) 
        {
            if (ctx->new_symbol < ctx->table_symbol[code])
            {
                if (ctx->table_left[code] != LZW_CODE_EMPTY) 
                    code = ctx->table_left[code];
                else 
                {
                    ctx->table_left[code] = ctx->new_code; /* insert code. */
                    break;
                }
            }
            else 
            {
                if (ctx->table_right[code] != LZW_CODE_EMPTY) 
                    code = ctx->table_right[code];
                else 
                {
                    ctx->table_right[code] = ctx->new_code; /* insert code. */
                    break;
                }
            }
        }
    }
    else 
        ctx->table_code[ctx->current_parent_code] = ctx->new_code;

    ctx->table_symbol[ctx->new_code] = ctx->new_symbol;

    return 1;
}
#else

/********* HASH *********/
void hash_free(lzw_context_enc *ctx)
{
    assert(ctx);

    if (ctx->table_code)
        free(ctx->table_code);
    if (ctx->table_parent)
        free(ctx->table_parent);
    if (ctx->table_symbol)
        free(ctx->table_symbol);
}

bool hash_init(lzw_context_enc *ctx)
{
    assert(ctx);

    ctx->hash_size = hash_sizes[ctx->code_max_bits - CODE_MIN_MAX_BITS];
    ctx->hash_shift = ctx->code_max_bits - 8;

    if (!(ctx->table_code = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_hash_enc;
    if (!(ctx->table_parent = calloc(1, sizeof(uint32_t) * ctx->hash_size)))
        goto abort_new_hash_enc;
    if (!(ctx->table_symbol = calloc(1, sizeof(uint8_t) * ctx->hash_size)))
        goto abort_new_hash_enc;
        
    return true;

    abort_new_hash_enc:
    hash_free(ctx);

    return false;
}

void hash_reset(lzw_context_enc *ctx)
{
    assert(ctx);

    for (int i = 0; i < ctx->hash_size; i++) 
        ctx->table_code[i] = LZW_CODE_EMPTY;
}

void hash_insert(lzw_context_enc *ctx, uint64_t index)
{
    assert(ctx && (ctx->table_code[index] == LZW_CODE_EMPTY));

    ctx->table_code[index]   = ctx->new_code;
    ctx->table_parent[index] = ctx->current_parent_code;
    ctx->table_symbol[index] = ctx->new_symbol;
}

FORCE_INLINE void hash_function_xor(lzw_context_enc *ctx, uint64_t* index)
{
    *index = ((uint64_t)ctx->new_symbol << ctx->hash_shift) ^ (uint64_t)ctx->current_parent_code;
}

FORCE_INLINE void hash_function_fnv(lzw_context_enc *ctx, uint64_t* index)
{
    //*index = (uint64_t)ctx->current_parent_code << 8 | (uint64_t)ctx->new_symbol;
    *index =  (uint64_t)ctx->new_symbol << ctx->code_max_bits | (uint64_t)ctx->current_parent_code;

    uint32_t hash = 2166136261U;
    char *data = (char*) index;
    hash = (16777619U * hash) ^ (uint8_t)(data[0]);
    hash = (16777619U * hash) ^ (uint8_t)(data[1]);
    hash = (16777619U * hash) ^ (uint8_t)(data[2]);
    hash = (16777619U * hash) ^ (uint8_t)(data[3]);

    *index = hash & lmask[ctx->code_max_bits];
}

static uint32_t crc32_tab[] =
{
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    0x2d02ef8dL
};

FORCE_INLINE void hash_function_crc32(lzw_context_enc *ctx, uint64_t* index)
{
    uint32_t value = 0xFFFFFFFF;
    *index =  (uint64_t)ctx->new_symbol << ctx->code_max_bits | (uint64_t)ctx->current_parent_code;
    //*index =  (uint64_t)ctx->current_parent_code << 8 | (uint64_t)ctx->new_symbol;
    char *data = (char*) index;

    value = ((value >> 8) & 0x00FFFFFF) ^ crc32_tab[(value ^ data[0]) & 0xFF];
    value = ((value >> 8) & 0x00FFFFFF) ^ crc32_tab[(value ^ data[1]) & 0xFF];
    value = ((value >> 8) & 0x00FFFFFF) ^ crc32_tab[(value ^ data[2]) & 0xFF];
    value = ((value >> 8) & 0x00FFFFFF) ^ crc32_tab[(value ^ data[3]) & 0xFF];

    value ^= 0xFFFFFFFF;

    *index = value & lmask[ctx->code_max_bits];
}

FORCE_INLINE void hash_function_jen(lzw_context_enc *ctx, uint64_t* index)
{
    uint32_t hash;
    uint32_t i;
    *index =  (uint64_t)ctx->new_symbol << ctx->code_max_bits | (uint64_t)ctx->current_parent_code;
    char *data = (char*) index;
    
    for( hash = i = 0; i < 4; ++i)
    {
        hash += data[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    *index = hash & lmask[ctx->code_max_bits];
}

int hash_lookup(lzw_context_enc *ctx, uint64_t* index)
{
    uint32_t offset;
    #ifdef HASH_STATISTICS
    uint8_t i_collision = 0;
    ctx->hashing++;
    #endif

    /*assert(ctx && index);*/

    hash_function_xor(ctx, index);
    offset = (*index) ? ((uint32_t)ctx->hash_size - *index) : (uint32_t)1;

    while (1)
    {
        if (ctx->table_code[*index] == LZW_CODE_EMPTY)
            return 0;

        if ((ctx->table_parent[*index] == ctx->current_parent_code)
            && (ctx->table_symbol[*index] == ctx->new_symbol))
        {
            #ifdef HASH_STATISTICS 
            (ctx->collisions[i_collision])++;
            #endif
            return 1;
        }
 
        if (*index < offset)
            *index += (uint32_t)ctx->hash_size - offset;
        else
            *index -= offset;

        #ifdef HASH_STATISTICS
        if (i_collision < 20)
            i_collision++;
        (ctx->tot_collisions)++;
        #endif
    }
}
#endif

void lzw_context_enc_delete(lzw_context_enc *ctx)
{
    if (ctx)
    {
        if (ctx->f_src)
            fclose(ctx->f_src);
        if (ctx->b_dst)
            bitio_close(ctx->b_dst);

        #ifdef USE_TRIE
        trie_free(ctx);
        #else
        hash_free(ctx);
        #endif

        memset(ctx, 0, sizeof(lzw_context_enc));
        free(ctx);
    }
}

static FORCE_INLINE void lzw_context_enc_reset(lzw_context_enc *ctx)
{
    assert(ctx);

    ctx->current_code_bits = 9;
    ctx->current_max_code  = 512;
    ctx->new_code = LZW_CODE_START;
    #ifdef USE_TRIE
    trie_reset(ctx);
    #else
    hash_reset(ctx);
    #endif
}

static FORCE_INLINE void lzw_context_enc_extend_codes(lzw_context_enc *ctx)
{
    ctx->current_code_bits++;
    ctx->current_max_code <<= 1;
}

lzw_context_enc *
lzw_context_enc_new(const char *src_file, const char *dst_file,
                    uint8_t ratio, bool binary_mode)
{
    lzw_context_enc *ctx = NULL;
    uint8_t max_bits = ratio + CODE_MIN_MAX_BITS;

    assert(src_file && dst_file);

    if (ratio > (CODE_MAX_MAX_BITS - CODE_MIN_MAX_BITS))
    {
        fprintf(stderr, "wrong compression ratio argument, "
                "setting to default: %d\n", CODE_MIN_MAX_BITS);
        max_bits = CODE_MIN_MAX_BITS;
    }

    if ( !(ctx = calloc(1, sizeof(struct lzw_context_enc))) ||
         !(ctx->f_src = fopen(src_file, "rb")) ||
         !(ctx->b_dst = bitio_open(dst_file, O_WRONLY)) )
        goto abort_new_context_enc;  /* Yeah, I'm a bad person */

    ctx->code_max_bits = max_bits;
    ctx->code_max = (uint32_t)(1 << ctx->code_max_bits);

    #ifdef USE_TRIE
    if (!trie_init(ctx))
        goto abort_new_context_enc;
    #else
    if (!hash_init(ctx))
        goto abort_new_context_enc;
    #endif

    ctx->table_max = ctx->code_max;
    ctx->binary_file = (uint8_t) binary_mode;

    /* header magic */
    bitio_write(ctx->b_dst, (uint64_t)HEADER_MAGIC, 24);
    /* lunghezza codifica massima */
    bitio_write(ctx->b_dst, (uint64_t)ctx->code_max_bits, 8);
    /* dimensio1ne reset tabella */
    bitio_write(ctx->b_dst, (uint64_t)ctx->table_max, 32);
    /* binary mode */
    bitio_write(ctx->b_dst, (uint64_t)ctx->binary_file, 1);

    lzw_context_enc_reset(ctx);
    printf("* max code bits       : %d\n", ctx->code_max_bits);
    return ctx;

    abort_new_context_enc:
    lzw_context_enc_delete(ctx);
    return NULL;
}

#ifdef USE_TRUNCATE_BIT_ENCODING
void truncated_binary_enc(lzw_context_enc *ctx)
{
    uint32_t u = ctx->current_max_code - ctx->new_code;
    if (ctx->current_parent_code < u)
        bitio_write(ctx->b_dst, (uint64_t)ctx->current_parent_code, ctx->current_code_bits-1);
    else
        bitio_write(ctx->b_dst, (uint64_t)(ctx->current_parent_code + u), ctx->current_code_bits);
}
#endif

FORCE_INLINE void write_fixed_code(lzw_context_enc *ctx)
{
    switch (ctx->current_code_bits)
    {
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            if (ctx->current_parent_code < 256) /* 8 bit */
            {
                bitio_write1(ctx->b_dst); /* 1 */
                bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 8);
            }
            else
            {
                bitio_write0(ctx->b_dst);
                #ifdef USE_TRUNCATE_BIT_ENCODING
                truncated_binary_enc(ctx);
                #else
                bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, ctx->current_code_bits);
                #endif
            }
            break;
        case 14:
        case 15:
        case 16:
        case 17:
            if (ctx->current_parent_code < 4096) /* 12 bit */
            {
                if (ctx->current_parent_code < 256) /* 8 bit */
                {
                    bitio_write(ctx->b_dst, (uint64_t) 3, 2); /* 11 */
                    bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 8);
                }
                else
                {
                    bitio_write(ctx->b_dst, (uint64_t) 2, 2); /* 10 */
                    bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 12);
                }
            }
            else
            {
                bitio_write0(ctx->b_dst);
                #ifdef USE_TRUNCATE_BIT_ENCODING
                truncated_binary_enc(ctx);
                #else
                bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, ctx->current_code_bits);
                #endif
            }
            break;
        case 18:
        case 19:
        case 20:
        case 21:
            if (ctx->current_parent_code < 65536) /* 16 bit */
            {
                if (ctx->current_parent_code < 4096) /* 12 bit */
                {
                    if (ctx->current_parent_code < 256) /* 8 bit */
                    {
                        bitio_write(ctx->b_dst, (uint64_t) 7, 3); /* 111 */
                        bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 8);
                    }
                    else
                    {
                        bitio_write(ctx->b_dst, (uint64_t) 6, 3); /* 110 */
                        bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 12);
                    }
                }
                else
                {
                    bitio_write(ctx->b_dst, (uint64_t) 2, 2); /* 10 */
                    bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 16);  
                }
            }
            else
            {
                bitio_write0(ctx->b_dst);
                #ifdef USE_TRUNCATE_BIT_ENCODING
                truncated_binary_enc(ctx);
                #else
                bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, ctx->current_code_bits);
                #endif
            }
            break;
        case 22:
        case 23:
        case 24:
        default: 
            if (ctx->current_parent_code < 1048575) /* 20 bit */
            {
                if (ctx->current_parent_code < 65536) /* 16 bit */
                {
                    if (ctx->current_parent_code < 4096) /* 12 bit */
                    {                
                        if (ctx->current_parent_code < 256) /* 8 bit */
                        {
                            bitio_write(ctx->b_dst, (uint64_t) 15, 4); /* 1111 */
                            bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 8);
                        }
                        else
                        {
                            bitio_write(ctx->b_dst, (uint64_t) 14, 4); /* 1110 */
                            bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 12);
                        }  
                    }
                    else
                    {
                        bitio_write(ctx->b_dst, (uint64_t) 6, 3); /* 110 */
                        bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 16);
                    }
                }
                else
                {
                    bitio_write(ctx->b_dst, (uint64_t) 2, 2); /* 10 */
                    bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, 20);
                }
            }
            else
            {
                bitio_write0(ctx->b_dst);
                #ifdef USE_TRUNCATE_BIT_ENCODING
                truncated_binary_enc(ctx);
                #else
                bitio_write(ctx->b_dst, (uint64_t) ctx->current_parent_code, ctx->current_code_bits);
                #endif
            }
            break;
    }
}

/********* compress function *********/
int compress_lzw(const char *src_file, const char *dst_file, uint8_t ratio, bool binary_mode)
{
    int ret = 0;
    #ifndef USE_TRIE
    uint64_t index;
    #endif
    lzw_context_enc *ctx = NULL;

    static char rd_block[READ_BLOCK_SIZE];
    int16_t rd_block_pos = 0, rd_block_last = 0;

    assert(src_file && dst_file);

    if (!(ctx = lzw_context_enc_new(src_file, dst_file, ratio, binary_mode)))
    {
        perror("lzw_new_context");
        return -1;
    }

    if (rd_block_pos == rd_block_last &&
    (rd_block_last = fread(rd_block, sizeof(char), READ_BLOCK_SIZE, ctx->f_src)) <= 0)
        exit(1);

    /* setto il nuovo carattere nel context */
    ctx->current_parent_code = (uint8_t)rd_block[rd_block_pos++];

    while (1)
    {
        if (rd_block_pos == rd_block_last)
        {
            /* quando finisce il file ritorna la read ritorna 0 ed esce */
            if ((rd_block_last = fread(rd_block, sizeof(char), READ_BLOCK_SIZE, ctx->f_src)) <= 0)
                break;
            rd_block_pos = 0;
        }

        /* setto il nuovo carattere nel context */
        ctx->new_symbol = (uint8_t)rd_block[rd_block_pos++];
        #ifdef USE_TRIE
        if (!trie_lookup(ctx))
        #else
        if (!hash_lookup(ctx, &index))
        #endif
        {
            if (ctx->binary_file)
            {
                write_fixed_code(ctx);
            }
            else
                #ifdef USE_TRUNCATE_BIT_ENCODING 
                truncated_binary_enc(ctx);
                #else
                bitio_write(ctx->b_dst, ctx->current_parent_code, ctx->current_code_bits);
                #endif

            if (ctx->new_code < ctx->code_max)
            {
                #ifdef USE_TRIE
                trie_insert(ctx);
                #else
                hash_insert(ctx, index);
                #endif
                if (ctx->new_code == ctx->current_max_code)
                    lzw_context_enc_extend_codes(ctx);
            }

            /* svuota tabella hash e resetta il contesto */
            if (ctx->new_code++ == ctx->table_max)
                lzw_context_enc_reset(ctx);

            /* aggiorno il parent all'index del nuovo simbolo */
            ctx->current_parent_code = ctx->new_symbol;
        }
        else
            #ifndef USE_TRIE
            ctx->current_parent_code = ctx->table_code[index];
            #endif
    }

    if (ctx->binary_file)
        write_fixed_code(ctx);
    else
        #ifdef USE_TRUNCATE_BIT_ENCODING
        truncated_binary_enc(ctx);
        #else
        bitio_write(ctx->b_dst, (uint64_t)ctx->current_parent_code, ctx->current_code_bits);
        #endif
    ctx->current_parent_code = (uint64_t)LZW_CODE_EOF;

    if (ctx->binary_file) 
        write_fixed_code(ctx);
    else
        #ifdef USE_TRUNCATE_BIT_ENCODING 
        truncated_binary_enc(ctx);
        #else
        bitio_write(ctx->b_dst, (uint64_t)LZW_CODE_EOF, ctx->current_code_bits);
        #endif

    #ifdef HASH_STATISTICS   
    printf("hashing statistics: %d \n", ctx->hashing);
    for (int i = 0; i < 21; i++)
        printf("collisions[%d]: %d \n", i, ctx->collisions[i]);
    printf("avg collisions: %f\n", (double)ctx->tot_collisions/(double)ctx->hashing);
    #endif

    lzw_context_enc_delete(ctx);
    return ret;
}
