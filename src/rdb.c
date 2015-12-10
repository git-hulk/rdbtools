#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include "util.h"
#include "lzf.h"
#include "intset.h"
#include "ziplist.h"
#include "zipmap.h"

#define MAGIC_STR "REDIS"
#define REDIS_EXPIRE_SEC 0xfd
#define REDIS_EXPIRE_MS 0xfc
#define REDIS_SELECT_DB 0xfe
#define REDIS_EOF 0xff

#define REDIS_RDB_STRING  0
#define REDIS_RDB_LIST    1
#define REDIS_RDB_SET     2
#define REDIS_RDB_ZSET    3
#define REDIS_RDB_HASH    4

#define REDIS_RDB_ZIPMAP  9
#define REDIS_RDB_LIST_ZIPLIST 10 
#define REDIS_RDB_INTSET  11 
#define REDIS_RDB_ZSET_ZIPLIST 12 
#define REDIS_RDB_HASH_ZIPLIST 13 

#define REDIS_RDB_6B 0
#define REDIS_RDB_14B 1
#define REDIS_RDB_32B 2
#define REDIS_RDB_ENCV 3
#define REDIS_RDB_LENERR UINT_MAX

#define REDIS_RDB_ENC_INT8 0
#define REDIS_RDB_ENC_INT16 1
#define REDIS_RDB_ENC_INT32 2
#define REDIS_RDB_ENC_LZF 3

/* ========================  ENCODE/DECODE ===================== */

/* =======================  RDB LOAD ====================== */
uint8_t
rdb_load_type(int fd)
{
    char buf[1];
    read(fd, buf, 1);

    return (uint8_t) buf[0];
}

// load string len
uint32_t
rdb_load_len(int fd, uint8_t *is_encoded)
{
    char buf[2];
    uint8_t type;
    uint32_t len;

   if (is_encoded) *is_encoded = 0;
    if (read(fd, buf, 1) == 0) return REDIS_RDB_LENERR;
    type = (buf[0] & 0xc0) >> 6; 

    /**
     * 00xxxxxx, then the next 6 bits represent the length
     * 01xxxxxx, then the next 14 bits represent the length
     * 10xxxxxx, then the next 6 bits is discarded, and next 4bytes represent the length 
     * 11xxxxxx, The remaining 6 bits indicate the format
     */
    if (REDIS_RDB_6B == type) {
        return (uint8_t)buf[0] & 0x3f;
    } else if (REDIS_RDB_14B == type) {
        if (read(fd, buf + 1, 1) == 0) return REDIS_RDB_LENERR;
        return (((uint8_t)buf[0] & 0x3f) << 8)| (uint8_t)buf[1];
    } else if (REDIS_RDB_32B == type) {
        if (read(fd, &len, 4) == 0) return REDIS_RDB_LENERR;
        return ntohl(len); 
    } else {
        if(is_encoded) *is_encoded = 1;
        return (uint8_t)buf[0] & 0x3f;
    }
}

int32_t
rdb_load_int(int fd, uint8_t enc)
{
    char buf[4];

    if (REDIS_RDB_ENC_INT8 == enc) {
        read(fd, buf, 1);
        return (int8_t)buf[0];
    } else if (REDIS_RDB_ENC_INT16 == enc) {
        read(fd, buf, 2);
        return (int16_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8));
    } else {
        read(fd, buf, 4);
        return (int32_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8) | ((uint8_t)buf[2] << 16) | ((uint8_t)buf[3] << 24));
    }
}

char *
rdb_load_lzf_string(int fd)
{
    int32_t clen, len;
    char *cstr, *str;
    /*
     * 1. load compress length.
     * 2. load raw length.
     * 3. load lzf_string, and use lzf_decompress to decode.
     */

    if((clen = rdb_load_len(fd, NULL)) == REDIS_RDB_LENERR)
        return NULL;
    if((len = rdb_load_len(fd, NULL)) == REDIS_RDB_LENERR)
        return NULL;

    cstr = malloc(clen);
    str = malloc(len);

    int ret;
    if ((ret = read(fd, cstr, clen)) == 0) goto err;
    if( (ret = lzf_decompress(cstr, clen, str, len)) == 0) goto err;
    str[len] = '\0';

    free(cstr);
    return str;

err:
    free(cstr);
    free(str);
    return NULL;
}

char *
rdb_load_string(int fd)
{
    uint8_t is_encoded;
    uint32_t len;
    char *buf;
    int32_t val;

    len = rdb_load_len(fd, &is_encoded);
    if (is_encoded) {
        switch (len) {

            case REDIS_RDB_ENC_INT8:
            case REDIS_RDB_ENC_INT16:
            case REDIS_RDB_ENC_INT32:
                val = rdb_load_int(fd, len);
                return ll2string(val);

            case REDIS_RDB_ENC_LZF:
                return rdb_load_lzf_string(fd);

             default:
                // unkonwn error.
                break;
        }
    }

    buf = malloc(len + 1);
    // FIXME: if len is too large?
    read(fd, buf, len);
    buf[len] = '\0';

    return buf;
}

void
rdb_load_list (int fd)
{
    int i, len;
    char *elem;

    len = rdb_load_len(fd, NULL); 
    printf("List { len : %d\n ", len);
    for (i = 0; i < len; i++) {
        elem = rdb_load_string(fd);
        printf("%s\n", elem);
        free(elem);
    }
    printf("}\n");
}

void
rdb_load_hash (int fd)
{
    int i, len;
    char *key , *val;

    len = rdb_load_len(fd, NULL); 
    printf("HASH { len : %d\n ", len);
    for (i = 0; i < len; i++) {
        key = rdb_load_string(fd);
        val = rdb_load_string(fd);
        printf("(%s,%s)", key, val);
        free(key);
        free(val);
    }
    printf("}\n");
}

void
rdb_load_obj(int fd, int type)
{
    if (REDIS_RDB_STRING == type) {
        rdb_load_string(fd);
    } else if (REDIS_RDB_INTSET == type) {
        char *is_str = rdb_load_string(fd);
        intset *is = (intset*) is_str; 
        intset_dump(is);
    } else if (REDIS_RDB_LIST_ZIPLIST == type) {
        char *zl_str = rdb_load_string(fd);
        ziplist_dump(zl_str);
    } else if (REDIS_RDB_ZIPMAP == type) {
        char *zm_str = rdb_load_string(fd);
        zipmap_dump(zm_str);
    } else if (REDIS_RDB_ZSET_ZIPLIST== type
            || REDIS_RDB_HASH_ZIPLIST == type) {
        char *zl_str = rdb_load_string(fd);
        ziplist_dump(zl_str);
    } else if (REDIS_RDB_LIST == type
            || REDIS_RDB_SET == type) {
        rdb_load_list(fd);
    } else if (REDIS_RDB_HASH == type
            || REDIS_RDB_ZSET == type) {
        rdb_load_hash(fd);
    }
}

uint32_t
rdb_load_time(int fd)
{
    char buf[4];
    uint32_t t32;

    read(fd, buf, 4);
    memcpy(&t32, buf, 4);

    return t32;
}

uint64_t
rdb_load_ms_time(int fd)
{
    char buf[8];
    uint64_t t64;

    read(fd, buf, 8);
    memcpy(&t64, buf, 8);

    return t64;
}

int
rdb_load(const char *path)
{
    char buf[128];
    int version;
    uint8_t type, db_num;
    uint64_t expire_time;

    if (access(path, R_OK) != 0) {
        return -1;
    }

    int rdb_fd = open(path, O_RDONLY);

    // read magic string(5bytes) and version(4bytes)
    read(rdb_fd, buf, 9);
    buf[9] = '\0';
    if(memcmp(buf, MAGIC_STR, 5) != 0) return -2;
    version = atoi(buf + 5);

    while (1) {
        expire_time = 0;
        type = rdb_load_type(rdb_fd);

        // load expire time if exists
        if (REDIS_EXPIRE_SEC == type) {
            // convert to millisecond
            expire_time = rdb_load_time(rdb_fd) * 1000; 
            // load value type
            type = rdb_load_type(rdb_fd);
        } else if (REDIS_EXPIRE_MS == type) {
            expire_time = rdb_load_ms_time(rdb_fd); 
            // load value type
            type = rdb_load_type(rdb_fd);
        }
        if(expire_time > 0) {
            printf("expire time is %llu.\n", expire_time);
        }

        // select db
        if (REDIS_SELECT_DB == type) {
            read(rdb_fd, buf, 1);
            db_num = (uint8_t)buf[0];
            printf("Select db %d\n", db_num);
            continue;
        }

        // end of rdb file
        if(REDIS_EOF == type) break;

        // read key
        char *key = rdb_load_string(rdb_fd);
        printf("key is %s\n", key);
        // read value
        rdb_load_obj(rdb_fd, type);

    }

    close(rdb_fd);

    return 0;
}

int
main(int argc, char **argv)
{
    printf("result: %d\n", rdb_load(argv[1]));
    return 0;
}
