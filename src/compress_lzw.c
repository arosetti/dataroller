#include "compress_lzw.h"
#include "bitio.h"
#include "shared.h"

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

#define READ_BLOCK_SIZE  8192

#define HEADER_MAGIC     0x00575a4c /* ZWL */

typedef struct lzw_context_enc
{
    /* si fanno separate per l'allineamento */
    uint32_t* table_code;    /* child */ /* FIXME child e parent insieme per < località */
    uint32_t* table_parent;  /* parent */
    uint8_t*  table_symbol;

    uint8_t  code_max_bits, hash_shift;
    uint32_t code_max, hash_size, table_max;

    struct bitio *b_dst;
    FILE  *f_src;
    uint32_t  current_parent_code;
    uint32_t  current_max_code;
    uint32_t  new_code;
    uint8_t   new_symbol;
    uint8_t   current_code_bits;
} lzw_context_enc;

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


int hash_lookup(lzw_context_enc *ctx, uint64_t* index)
{
    uint32_t offset;

    hash_function_xor(ctx, index);
    offset = (*index) ? ((uint32_t)ctx->hash_size - *index) : (uint32_t)1;

    while (1)
    {
        if (ctx->table_code[*index] == LZW_CODE_EMPTY)
            return 0;

        if ((ctx->table_parent[*index] == ctx->current_parent_code)
            && (ctx->table_symbol[*index] == ctx->new_symbol))
        {
            return 1;
        }
 
        if (*index < offset)
            *index += (uint32_t)ctx->hash_size - offset;
        else
            *index -= offset;
    }
}

void lzw_context_enc_delete(lzw_context_enc *ctx)
{
    if (ctx)
    {
        if (ctx->f_src)
            fclose(ctx->f_src);
        if (ctx->b_dst)
            bitio_close(ctx->b_dst);

        hash_free(ctx);

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
    hash_reset(ctx);
}

static FORCE_INLINE void lzw_context_enc_extend_codes(lzw_context_enc *ctx)
{
    ctx->current_code_bits++;
    ctx->current_max_code <<= 1;
}

lzw_context_enc *
lzw_context_enc_new(const char *src_file, const char *dst_file,
                    uint8_t ratio)
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

    if (!hash_init(ctx))
        goto abort_new_context_enc;

    ctx->table_max = ctx->code_max;

    /* header magic */
    bitio_write(ctx->b_dst, (uint64_t)HEADER_MAGIC, 24);
    /* lunghezza codifica massima */
    bitio_write(ctx->b_dst, (uint64_t)ctx->code_max_bits, 8);
    /* dimensio1ne reset tabella */
    bitio_write(ctx->b_dst, (uint64_t)ctx->table_max, 32);

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

/********* compress function *********/
int compress_lzw(const char *src_file, const char *dst_file, uint8_t ratio)
{
    int ret = 0;
    uint64_t index;
    lzw_context_enc *ctx = NULL;

    static char rd_block[READ_BLOCK_SIZE];
    int16_t rd_block_pos = 0, rd_block_last = 0;

    assert(src_file && dst_file);

    if (!(ctx = lzw_context_enc_new(src_file, dst_file, ratio)))
    {
        perror("lzw_new_context");
        return -1;
    }

    /* leggiamo il primo blocco di byte dal file caricando il buffer locale */
    if (rd_block_pos == rd_block_last &&
    (rd_block_last = fread(rd_block, sizeof(char), READ_BLOCK_SIZE, ctx->f_src)) <= 0)
        exit(1);

    /* setto il nuovo carattere nel context */
    ctx->current_parent_code = (uint8_t)rd_block[rd_block_pos++];

    while (1)
    {
        /* il buffer locale è finito, carichiamo un nuovo blocco di byte dal file */
        if (rd_block_pos == rd_block_last)
        {
            /* quando finisce il file ritorna la read ritorna 0 ed esce */
            if ((rd_block_last = fread(rd_block, sizeof(char), READ_BLOCK_SIZE, ctx->f_src)) <= 0)
                break;
            rd_block_pos = 0;
        }

        /* setto il nuovo carattere nel context */
        ctx->new_symbol = (uint8_t)rd_block[rd_block_pos++];
        /* ricerca nell'hash */
        if (!hash_lookup(ctx, &index))
        {
            /* scrivo il parent_code nella bitio */
#ifdef USE_TRUNCATE_BIT_ENCODING 
            truncated_binary_enc(ctx);
#else
            bitio_write(ctx->b_dst, ctx->current_parent_code, ctx->current_code_bits);
#endif

            if (ctx->new_code < ctx->code_max)
            {
                hash_insert(ctx, index);
                if (ctx->new_code == ctx->current_max_code)
                    lzw_context_enc_extend_codes(ctx);
            }

            /* svuota tabella hash e resetta il contesto */
            if (ctx->new_code++ == ctx->table_max)
                lzw_context_enc_reset(ctx);

            /* aggiorno il parent all'index del nuovo simbolo */
            ctx->current_parent_code = ctx->new_symbol;
        }
        else /* aggiorno il parent_code con il code trovato nell'hashtable */
            ctx->current_parent_code = ctx->table_code[index];
    }
    
    /* il file è finito scriviamo l'ultimo parent_code */
#ifdef USE_TRUNCATE_BIT_ENCODING
    truncated_binary_enc(ctx);
#else
    bitio_write(ctx->b_dst, (uint64_t)ctx->current_parent_code, ctx->current_code_bits);
#endif
    ctx->current_parent_code = (uint64_t)LZW_CODE_EOF;

    /* scriviamo il codice di EOF */
#ifdef USE_TRUNCATE_BIT_ENCODING 
    truncated_binary_enc(ctx);
#else
    bitio_write(ctx->b_dst, (uint64_t)ctx->current_parent_code, ctx->current_code_bits);
#endif

    /* liberiamo la memoria deallocando il contesto */
    lzw_context_enc_delete(ctx);
    return ret;
}
