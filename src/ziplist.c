#include "ziplist.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "endian.h"

#define ZIPLIST_BIGLEN 254
#define ZIPLIST_END 0xff

#define ZIP_ENC_INT8  0xfe
#define ZIP_ENC_INT16 0xc0
#define ZIP_ENC_INT24 0xf0
#define ZIP_ENC_INT32 0xd0
#define ZIP_ENC_INT64 0xe0

#define ZIP_ENC_STR_6B  (0 << 6)
#define ZIP_ENC_STR_14B (1 << 6)
#define ZIP_ENC_STR_32B (2 << 6)

#define ZIP_ENC_STR_MASK 0xc0
#define ZIP_IS_END(entry) ((uint8_t)entry[0] == ZIPLIST_END)

uint32_t ziplist_entry_size(const char *s);

uint32_t
ziplist_prev_len_size(const char *s)
{
    if ((uint8_t)s[0] < ZIPLIST_BIGLEN) 
        return 1;
    else
        return 5;
}

uint8_t
ziplist_entry_is_str(const char *entry)
{
    uint8_t enc;
    enc = entry[ziplist_prev_len_size(entry)];
    enc &= ZIP_ENC_STR_MASK;
    if (enc == ZIP_ENC_STR_6B || enc == ZIP_ENC_STR_14B
      || enc == ZIP_ENC_STR_32B) {
        return 1;
    } else {
        return 0;
    }
}

uint32_t
ziplist_entry_strlen(const char *entry)
{
    uint8_t enc;
    uint32_t pos;
    
    pos = ziplist_prev_len_size(entry);
    enc = (uint8_t)entry[pos] & 0xc0;
    if (enc == ZIP_ENC_STR_6B) {
        return entry[pos] & ~ZIP_ENC_STR_MASK;
    } else if (enc == ZIP_ENC_STR_14B) {
        uint32_t ret = (((uint8_t)entry[pos] & ~ZIP_ENC_STR_MASK) << 8) | (uint8_t)entry[pos + 1];
        return ret;
    } else if (enc == ZIP_ENC_STR_32B) {
        return ((uint8_t)entry[pos + 1] << 24) | ((uint8_t)entry[pos + 2] << 16) | ((uint8_t)entry[pos + 3] << 8) | (uint8_t)entry[pos + 4];
    }
    return 0;
}

uint32_t
ziplist_entry_size(const char *entry)
{
    uint8_t enc;
    uint32_t size = 1, pos;

    pos = ziplist_prev_len_size(entry);
    enc = entry[pos];
    // prev entry length
    size += ziplist_prev_len_size(entry); 

    if(enc == ZIP_ENC_INT8) {
        size += 1;
    } else if (enc == ZIP_ENC_INT16) {
        size += 2;
    } else if (enc == ZIP_ENC_INT24) {
        size += 3;
    } else if (enc == ZIP_ENC_INT32) {
        size += 4;
    } else if (enc == ZIP_ENC_INT64) {
        size += 8;
    } else if ((enc & ZIP_ENC_STR_MASK) == ZIP_ENC_STR_6B) {
        size += ziplist_entry_strlen(entry);
    } else if ((enc & ZIP_ENC_STR_MASK) == ZIP_ENC_STR_14B) {
        size += 1;
        size += ziplist_entry_strlen(entry);
    } else if ((enc & ZIP_ENC_STR_MASK) == ZIP_ENC_STR_32B) {
        size += 4;
        size += ziplist_entry_strlen(entry);
    }
    
    return size;
}

char*
ziplist_entry_str(const char *entry)
{
    uint8_t enc;
    uint32_t pre_len_size,len_size = 1, slen;
    char *content, *str = NULL;

    pre_len_size = ziplist_prev_len_size(entry);
    enc = entry[pre_len_size] & ZIP_ENC_STR_MASK;
    if (enc == ZIP_ENC_STR_14B) len_size = 2;
    if (enc == ZIP_ENC_STR_32B) len_size = 5;

    content = (char *)entry + pre_len_size + len_size;
    if (enc == ZIP_ENC_STR_6B || enc == ZIP_ENC_STR_14B
      || enc == ZIP_ENC_STR_32B) {
        
        slen = ziplist_entry_strlen(entry);
        str = malloc(slen + 1); 
        memcpy(str, content, slen);
        str[slen] = '\0';
    }
    return str;
}

uint8_t
ziplist_entry_int(const char *entry, int64_t *v)
{
    int8_t  v8;
    int16_t v16;
    int32_t v32;
    int64_t v64;

    uint8_t enc;
    uint32_t pre_len_size;
    char *content;
    
    pre_len_size = ziplist_prev_len_size(entry);
    content = (char *)entry + pre_len_size;
    enc = entry[pre_len_size];
    
    // add one byte for encode.
    if (enc == ZIP_ENC_INT8) {
        memcpy(&v8, content + 1, sizeof(int8_t));
        *v = v8;
        return 1;
    } else if (enc == ZIP_ENC_INT16) {
        memcpy(&v16, content + 1, sizeof(int16_t));
        memrev16ifbe(&v16);
        *v = v16;
        return 1;
    } else if (enc == ZIP_ENC_INT24) {
        memcpy(&v32, content + 1, 3);
        memrev32ifbe(&v32);
        *v = v32;
        return 1;
    } else if (enc == ZIP_ENC_INT32) {
        memcpy(&v32, content + 1, sizeof(int32_t));
        memrev32ifbe(&v32);
        *v = v32;
        return 1;
    } else if (enc == ZIP_ENC_INT64){
        memcpy(&v64, content + 1, sizeof(int64_t));
        memrev64ifbe(&v64);
        *v = v64;
        return 1;
    } else if ((enc & 0xf0) == 0xf0){
        v8 = content[0] & 0x0f;
        *v = v8;
        return 1;
    }
  
    return  0;
}

void
ziplist_dump(const char *s)
{
    uint32_t i = 0, len;

    printf("ziplist { \n");
    printf("bytes: %u\n", ZL_BYTES(s));
    printf("len: %u\n", ZL_LEN(s));
    len = ZL_LEN(s);
    char *entry = (char *)ZL_ENTRY(s);
    while (!ZIP_IS_END(entry)) {
        if (ziplist_entry_is_str(entry)) {
            printf("str value: %s\n", ziplist_entry_str(entry)); 
        } else {
            int64_t v;
            ziplist_entry_int(entry, &v);
            printf("int value: %lld\n", v);
        }
        entry += ziplist_entry_size(entry);
        ++i;
    }
    printf("}\n");
    if(i < (0xffff - 1) && i != len) {
        printf("====== Ziplist len error. ======\n");
        exit(1);
    }
}
