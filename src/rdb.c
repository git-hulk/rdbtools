#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <arpa/inet.h>
#include "util.h"
#include "lzf.h"
#include "intset.h"
#include "ziplist.h"
#include "zipmap.h"
#include "script.h"
#include "crc64.h"
#include "log.h"
#include "endian.h"

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

uint64_t cksum = 0;
 // NOTE: trick here, version set 5 as we want to calc crc where read version field.
int version = 5;

ssize_t    
crc_read(int fildes, void *buf, size_t nbyte)
{
    ssize_t len;
    len = read(fildes, buf, nbyte);

    if (version >= 5) {
        cksum = crc64(cksum, buf, len);
    }

    return len;
}

/* =======================  RDB LOAD ====================== */
uint8_t
rdb_load_type(int fd)
{
    char buf[1];
    if(crc_read(fd, buf, 1) == 0) {
        logger(ERROR, "Exited, as read error on load type.\n");
        exit(1);
    }

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
    if (crc_read(fd, buf, 1) == 0) return REDIS_RDB_LENERR;
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
        if (crc_read(fd, buf + 1, 1) == 0) return REDIS_RDB_LENERR;
        return (((uint8_t)buf[0] & 0x3f) << 8)| (uint8_t)buf[1];
    } else if (REDIS_RDB_32B == type) {
        if (crc_read(fd, &len, 4) == 0) return REDIS_RDB_LENERR;
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
        if (crc_read(fd, buf, 1) == 0) goto READERR;
        return (int8_t)buf[0];
    } else if (REDIS_RDB_ENC_INT16 == enc) {
        if (crc_read(fd, buf, 2) == 0) goto READERR;
        return (int16_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8));
    } else {
        if (crc_read(fd, buf, 4) == 0) goto READERR;
        return (int32_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8) | ((uint8_t)buf[2] << 16) | ((uint8_t)buf[3] << 24));
    }
READERR:
    logger(ERROR, "Exited, as read error on load integer.\n");
    exit(1);
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
    str = malloc(len+1);
    if (!cstr || !str) {
        logger(ERROR, "Exited, as malloc failed at load lzf string.\n");
        exit(1);
    }

    int ret;
    if ((ret = crc_read(fd, cstr, clen)) == 0) goto err;
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
                logger(ERROR, "Exited, as error on load string.\n");
        }
    }

    int i = 0, bytes;
    buf = malloc(len + 1);
    if (!buf) {
        logger(ERROR, "Exited, as malloc failed at load string.\n");
    }
    while ( i < len && (bytes = crc_read(fd, buf + i, len - i))) {
        i += bytes;
    }
    buf[len] = '\0';

    return buf;
}

void
push_list (lua_State *L, int fd)
{
    int i, len;
    char *elem;

    len = rdb_load_len(fd, NULL); 
    for (i = 0; i < len; i++) {
        elem = rdb_load_string(fd);
        script_push_list_elem(L, elem, i);
        free(elem);
    }
}

void
push_hash (lua_State *L, int fd)
{
    int i, len;
    char *key , *val;

    len = rdb_load_len(fd, NULL); 
    for (i = 0; i < len; i++) {
        key = rdb_load_string(fd);
        val = rdb_load_string(fd);
        script_pushtablestring(L, key, val);
        free(key);
        free(val);
    }
}

void
rdb_load_value(lua_State *L, int fd, int type)
{
    int i;
    int64_t v64;
    char *str = NULL, *val_type = "string";
    char *s64;

    if (REDIS_RDB_STRING == type) {
        str = rdb_load_string(fd);
        script_pushtablestring(L, "value", str);
        script_pushtablestring(L, "type", val_type);
        free(str);
        return;
    }
    
    lua_pushstring(L, "value");
    lua_newtable(L);
    if (REDIS_RDB_INTSET == type) {
        str = rdb_load_string(fd);
        intset *is = (intset*) str;
        for (i = 0; i< is->length; i++) {
            intset_get(is, i, &v64);
            s64 = ll2string(v64); 
            script_push_list_elem(L, s64, i);
            free(s64);
        }

        val_type = "set";
    } else if (REDIS_RDB_LIST_ZIPLIST == type) {
        str = rdb_load_string(fd);
        push_ziplist_list_or_set(L, str);
        val_type = "list";
    } else if (REDIS_RDB_ZIPMAP == type) {
        str = rdb_load_string(fd);
        push_zipmap(L, str);
        val_type = "hash";
    } else if (REDIS_RDB_ZSET_ZIPLIST== type
            || REDIS_RDB_HASH_ZIPLIST == type) {
        str = rdb_load_string(fd);
        push_ziplist_hash_or_zset(L, str);
        val_type = REDIS_RDB_ZSET_ZIPLIST== type? "zset" : "hash";
    } else if (REDIS_RDB_LIST == type
            || REDIS_RDB_SET == type) {
        push_list(L, fd);
        val_type = REDIS_RDB_LIST == type? "list" : "set";
    } else if (REDIS_RDB_HASH == type
            || REDIS_RDB_ZSET == type) {
        push_hash(L, fd);
        val_type = REDIS_RDB_HASH == type? "hash" : "zset";
    }

    lua_settable(L,-3);
    script_pushtablestring(L, "type", val_type);

    if(str) free(str);
}

uint32_t
rdb_load_time(int fd)
{
    char buf[4];
    uint32_t t32;

    if( crc_read(fd, buf, 4) == 0) {
        logger(ERROR, "Exited, as read error on load time.\n");
    }
    memcpy(&t32, buf, 4);

    return t32;
}

uint64_t
rdb_load_ms_time(int fd)
{
    char buf[8];
    uint64_t t64;

    if( crc_read(fd, buf, 8) == 0) {
        logger(ERROR, "Exited, as read error on load microtime.\n");
    }
    memcpy(&t64, buf, 8);

    return t64;
}

int
rdb_load(lua_State *L, const char *path)
{
    char buf[128];
    uint8_t type, db_num;
    int expire_time;

    int rdb_fd = open(path, O_RDONLY);
    // read magic string(5bytes) and version(4bytes)
    if(crc_read(rdb_fd, buf, 9) == 0) {
        logger(ERROR,"Exited, as read error on laod version\n");
    }
    buf[9] = '\0';
    if(memcmp(buf, MAGIC_STR, 5) != 0) return -2;
    version = atoi(buf + 5);

    lua_getglobal(L, RDB_ENV);
    script_pushtableinteger(L, "version", version);
    lua_pop(L,-1);


    while (1) {
        expire_time = -1;
        type = rdb_load_type(rdb_fd);

        // load expire time if exists
        if (REDIS_EXPIRE_SEC == type) {
            // convert to millisecond
            expire_time = rdb_load_time(rdb_fd); 
            // load value type
            type = rdb_load_type(rdb_fd);
        } else if (REDIS_EXPIRE_MS == type) {
            expire_time = rdb_load_ms_time(rdb_fd) / 1000; 
            // load value type
            type = rdb_load_type(rdb_fd);
        }

        // select db
        if (REDIS_SELECT_DB == type) {
            if (crc_read(rdb_fd, buf, 1) == 0) {
                logger(ERROR, "Exited, as read error on laod db num.\n");
            }
            db_num = (uint8_t)buf[0];
            lua_getglobal(L, RDB_ENV);
            script_pushtableinteger(L, "db_num", db_num);
            lua_pop(L,-1);
            continue;
        }

        // end of rdb file
        if(REDIS_EOF == type) break;

        // read key
        char *key = rdb_load_string(rdb_fd);
        if (key) {
            lua_getglobal(L, RDB_CB); 
            lua_newtable(L);
            script_pushtablestring(L, "key", key);
            script_pushtableinteger(L, "expire_time", expire_time);

            // read value
            rdb_load_value(L, rdb_fd, type);
            if( lua_pcall(L, 1, 0, 0) != 0 ) {
                logger(ERROR, "Error running function `f': %s",
                        lua_tostring(L, -1));
            }
                
        }

        free(key);
    }

    if (version >= 5) {
        uint64_t expected_crc = 0;
        read(rdb_fd, &expected_crc, 8);
        memrev64ifbe(expected_crc);
        if(cksum != expected_crc) {
            logger(ERROR, "checksum error, expect %llu, real %llu.\n", cksum, expected_crc);
        }
    }
    close(rdb_fd);

    return 0;
}
