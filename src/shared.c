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
