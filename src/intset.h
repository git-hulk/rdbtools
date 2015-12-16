#ifndef _INTSET_H_
#define _INTSET_H_
#include <stdint.h>
typedef struct {
    uint32_t encoding; /* int16, int32 or int64 */
    uint32_t length;
    uint8_t contents[0];
} intset;

void intset_dump(intset *is);
uint8_t intset_get(intset *is, int pos, int64_t *v);
#endif
