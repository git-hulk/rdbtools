#ifndef _ZIPLIST_H_
#define _ZIPLIST_H_
#include <stdint.h>

#define ZL_BYTES(zl) *((uint32_t *)(zl))
#define ZL_LEN(zl) *((uint16_t*)((zl) + 2 * sizeof(uint32_t)))
#define ZL_HDR_SIZE (2 * sizeof(uint32_t) + sizeof(uint16_t))
#define ZL_ENTRY(zl) ((uint8_t *)zl + ZL_HDR_SIZE)

typedef struct {
    uint32_t bytes;
    uint32_t len;
    uint32_t tail;
    char entrys[0];
} ziplist;

void ziplist_dump(const char *s);
#endif
