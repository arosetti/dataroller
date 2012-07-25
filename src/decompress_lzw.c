#include "decompress_lzw.h"
#include "bitio.h"
#include "shared.h"

#define CODE_MIN_MAX_BITS  12
#define CODE_MAX_MAX_BITS  26

#define LZW_CODE_EMPTY    256
#define LZW_CODE_EOF      257
#define LZW_CODE_START    258

/* prime number bigger than ctx->code_max      */
/* http://primes.utm.edu/lists/small/millions/ */
static const uint32_t table_sizes[15] = 
{
    4096,         /* CODE_MAX_BITS == 12 min: 4095*/
    8192,         /* CODE_MAX_BITS == 13 min: 8191*/
    16384,        /* CODE_MAX_BITS == 14 min: 16383*/
    32768,        /* CODE_MAX_BITS == 15 min: 32767*/
    65536,        /* CODE_MAX_BITS == 16 min: 65535*/
    131072,       /* CODE_MAX_BITS == 17 min: 131071*/
    262144,       /* CODE_MAX_BITS == 18 min: 262143*/
    524288,       /* CODE_MAX_BITS == 19 min: 524287*/
    1048576,      /* CODE_MAX_BITS == 20 min: 1048575*/
    2097152,      /* CODE_MAX_BITS == 21 min: 2097151*/
    4194304,      /* CODE_MAX_BITS == 22 min: 4194303*/
    8388608,      /* CODE_MAX_BITS == 23 min: 8388607*/
    16777216,     /* CODE_MAX_BITS == 24 min: 16777215*/
    33554432,     /* CODE_MAX_BITS == 25 min: 33554431*/
    67108864      /* CODE_MAX_BITS == 26 min: 67108863*/
};

#define WR_BUFFER_SIZE  8192

#define HEADER_MAGIC     0x00575a4c /* ZWL */

typedef struct lzw_context_dec
{
    uint32_t*      table_parent;
    uint8_t*       table_symbol;
    FILE*          f_dst;
    struct bitio*  b_src;

    uint8_t  code_max_bits;
    uint32_t code_max, table_size, table_max;

    uint8_t  current_code_bits;
    uint32_t current_max_code;
    uint32_t current_code;
    uint32_t old_code;
    uint32_t new_code;
    uint32_t cnt_code;

    uint32_t truncate_code;

    int      cnt_stack;
    uint8_t *stack_buffer, *stack;
} lzw_context_dec;

void lzw_context_dec_delete(lzw_context_dec *ctx)
{
    if (ctx)
    {
        if (ctx->f_dst)
            fclose(ctx->f_dst);
        if (ctx->b_src)
            bitio_close(ctx->b_src);
        if (ctx->table_parent)
            free(ctx->table_parent);
        if (ctx->table_symbol)
            free(ctx->table_symbol);
        if (ctx->stack_buffer)
            free(ctx->stack_buffer);

        memset(ctx, 0, sizeof(lzw_context_dec));
        free(ctx);
    }
}

static inline void lzw_context_dec_reset(lzw_context_dec *ctx)
{
    assert(ctx);

    ctx->current_code_bits = 9;
    ctx->current_max_code  = 512;
    ctx->cnt_code = LZW_CODE_START;
    ctx->truncate_code = LZW_CODE_START;
}

static inline void lzw_context_dec_extend_codes(lzw_context_dec *ctx)
{
    ctx->current_code_bits++;
    ctx->current_max_code <<= 1;
}

lzw_context_dec *lzw_context_dec_new(const char *src_file, const char *dst_file)
{
    lzw_context_dec *ctx = NULL;
    uint64_t data;

    if ( !(ctx = calloc(1, sizeof(struct lzw_context_dec))) ||
         !(ctx->f_dst = fopen(dst_file, "wb")) ||
         !(ctx->b_src = bitio_open(src_file, O_RDONLY)) )
        goto abort_new_context_dec;

    /* lettura header magic */
    if (bitio_read(ctx->b_src, &data, 24) != 0)
        goto abort_new_context_dec;
    if ((uint32_t)data != HEADER_MAGIC)
    {
        errno = EINVAL;
        fprintf(stderr, "\"%s\" doesn't seem to be a valid LZW file...\n", src_file);
        goto abort_new_context_dec;
    }

    /*  lettura max bits */
    if (bitio_read(ctx->b_src, &data, 8) != 0)
        goto abort_new_context_dec;
    ctx->code_max_bits = (uint8_t)data;

    ctx->code_max = (uint32_t)(1 << ctx->code_max_bits);
    ctx->table_size = table_sizes[ctx->code_max_bits - CODE_MIN_MAX_BITS];
    printf("* max code bits       : %d\n", ctx->code_max_bits);

    if (!(ctx->table_parent = calloc(1, sizeof(uint32_t) * ctx->table_size)))
        goto abort_new_context_dec;
    if (!(ctx->table_symbol = calloc(1, sizeof(uint8_t) * ctx->table_size)))
        goto abort_new_context_dec;

    if (!(ctx->stack_buffer = calloc(1, sizeof(uint8_t) * ctx->code_max)))
        goto abort_new_context_dec;

    ctx->table_max = (uint32_t)data;
    /*  lettura dimensione reset tabella */
    if (bitio_read(ctx->b_src, &data, 32) != 0)
        goto abort_new_context_dec;
    ctx->table_max = (uint32_t)data;

    lzw_context_dec_reset(ctx);

    return ctx;

    abort_new_context_dec:
    lzw_context_dec_delete(ctx);

    return NULL;
}

static void table_insert(lzw_context_dec *ctx, int prefix_code, unsigned char symbol)
{
    ctx->table_parent[ctx->cnt_code] = prefix_code;
    ctx->table_symbol[ctx->cnt_code] = symbol;
}

static FORCE_INLINE int buffering_write(lzw_context_dec *ctx, char *buf, int32_t *pos, unsigned char* symbol)
{
    /* se il buffer è pieno scriviamo il blocco di byte sul file decompresso. */
    if (*pos == WR_BUFFER_SIZE)
    {
        if ((fwrite(buf, sizeof(char), *pos, ctx->f_dst)) <= 0)
            return 1;
        *pos = 0;
    }
    buf[(*pos)++] = *symbol;
    return 0;
}

#ifdef USE_TRUNCATE_BIT_ENCODING
void truncated_binary_dec(lzw_context_dec *ctx, uint64_t *data)
{
    uint64_t temp_data = 0;
    uint64_t u = ctx->current_max_code - ctx->truncate_code;

    ctx->truncate_code++;

    if (bitio_read(ctx->b_src, data, (ctx->current_code_bits-1)) != 0)
        exit(1);

    if (*data < u)
        return;
    else
    {
        if (bitio_read(ctx->b_src, &temp_data, 1) != 0)
            exit(1);

        *data = (*data << 1) | temp_data;
        *data -= u; 
    }
}
#endif

void FORCE_INLINE get_code(lzw_context_dec *ctx, uint64_t* data)
{    
#ifdef USE_TRUNCATE_BIT_ENCODING
    truncated_binary_dec(ctx, data);
#else
    if (bitio_read(ctx->b_src, data, ctx->current_code_bits) != 0)
        exit(1);
#endif
}

/********* decompress function *********/
int decompress_lzw(const char *src_file, const char *dst_file)
{
    int ret = 0;
    uint64_t data;
    lzw_context_dec *ctx = NULL;

    static char wr_buffer[WR_BUFFER_SIZE]; /* il buffer */
    int32_t wr_buffer_pos = 0;

    if (!(ctx = lzw_context_dec_new(src_file, dst_file)))
    {
        perror("lzw_new_context");
        return -1;
    }

    /* get first code. */
    get_code(ctx,&data);
    ctx->old_code = (uint32_t)data;

    wr_buffer[wr_buffer_pos++] = (char)ctx->old_code;

    while (1)
    {
        get_code(ctx,&data);
        ctx->new_code = (uint32_t)data;

        if (ctx->new_code == LZW_CODE_EOF)  /* codice fine file ricevuto */
            break;
        else if (ctx->new_code >= ctx->cnt_code)
            ctx->current_code = ctx->old_code;
        else 
            ctx->current_code = ctx->new_code;

        ctx->stack = ctx->stack_buffer;

        while ( ctx->current_code > LZW_CODE_EOF )
        {
            *(ctx->stack) = ctx->table_symbol[ctx->current_code];
            (ctx->stack)++;
            /* when while exits, current_code is a character */
            ctx->current_code = ctx->table_parent[ctx->current_code];
        }

        *(ctx->stack) = ctx->current_code;

        while (ctx->stack >= ctx->stack_buffer)
        {
            buffering_write(ctx, wr_buffer, &wr_buffer_pos, (unsigned char*)ctx->stack);

            if (ctx->stack != ctx->stack_buffer)
                (ctx->stack)--;
            else 
                break;
        }

        if (ctx->new_code >= ctx->cnt_code) /* undefined code */
        {
            buffering_write(ctx, wr_buffer, &wr_buffer_pos, (unsigned char*) &ctx->current_code);
        }

        if ( ctx->cnt_code < ctx->code_max ) /* add prev code + k to the table */
        {
            table_insert(ctx, ctx->old_code, (unsigned char)ctx->current_code);
            if (ctx->current_code_bits < ctx->code_max_bits)
            {
                if ( ctx->cnt_code == (ctx->current_max_code-1) ) 
                    lzw_context_dec_extend_codes(ctx);
            }
        }

        ctx->old_code = ctx->new_code; /* prev code = cur code */

        if (++(ctx->cnt_code) == ctx->table_max) /* resetting table */
        {
            lzw_context_dec_reset(ctx);
            
#ifdef USE_TRUNCATE_BIT_ENCODING
            truncated_binary_dec(ctx, &data);
#else
            if (bitio_read(ctx->b_src, &data, ctx->current_code_bits) != 0)
                exit(1);
#endif
            ctx->old_code = (uint32_t)data;

            buffering_write(ctx, wr_buffer, &wr_buffer_pos, (unsigned char*) &ctx->old_code); /* first code is a character */
        }
    }

    if (wr_buffer_pos && (fwrite(wr_buffer, sizeof(char), wr_buffer_pos, ctx->f_dst) <= 0)) /* scrive il resto del blocco */
        exit(1);

    lzw_context_dec_delete(ctx);
    return ret;
}
