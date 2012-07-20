#include "shared.h"

void *my_malloc(size_t size)
{
    void *p;
    if ( !(p = malloc(size)) )
    {
        perror("malloc");
        exit(1);
    }
    return p;
}

void *my_calloc(size_t n_elem, size_t size)
{
    void *p;
    if ( !(p = calloc(n_elem, size)) )
    {
        perror("malloc");
        exit(1);
    }
    return p;
}
/*
bool info(const char *fmt, ...)
{
    char* buffer;
    int ret;

    assert(fmt);

    va_list ap;
    va_start(ap, fmt);
    ret = vasprintf(&buffer, fmt, ap);
    va_end(ap);

    if (ret)
    {
        printf("* %s\n", buffer);
        free(buffer);
        return true;
    }

    perror("vasprintf");
    return false;
}
*/
