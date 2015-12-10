#include "intset.h"
#include <stdio.h> 
#include <string.h> 

void
intset_dump(intset *is)
{
    printf("encoding: %d\n", is->encoding);
    printf("length: %d\n", is->length);

    int i;
    int64_t v;
    printf("element { ");
    for (i = 0; i < is->length; i++) {
        intset_get(is, i, &v);
        printf("%lld\t", v);
    }
    printf("}\n");
}

uint8_t
intset_get(intset *is, int pos, int64_t *v)
{
    int64_t v64;
    int32_t v32;
    int16_t v16;

    if(pos >= is->length) return 0;

    if (is->encoding == sizeof(int64_t)) {
        memcpy(&v64, (int64_t*)is->contents + pos, sizeof(int64_t));
        *v = v64;
    } else if (is->encoding == sizeof(int32_t)) {
        memcpy(&v32, (int32_t*)is->contents + pos, sizeof(int32_t));
        *v = v32;
    } else {
        memcpy(&v16, (int16_t*)is->contents + pos, sizeof(int16_t));
        *v = v16;
    }
    return 1;
}
