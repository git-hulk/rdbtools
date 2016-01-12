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

#define KEY_FIELD_STR "key"
#define VAL_FIELD_STR "value"
#define TYPE_FIELD_STR "type"
#define EXP_FIELD_STR "expire_time" 
#define DB_NUM_STR "db_num"
#define VERSION_STR "version"

uint64_t cksum = 0;
 // NOTE: trick here, version set 5 as we want to calc crc where read version field.
int version = 5;


// ================================== COMMON UTIL FOR RDB. ==================================== //

static ssize_t    
rdb_crc_read(int fildes, void *buf, size_t nbyte)
{
    ssize_t len;
    len = read(fildes, buf, nbyte);

    if (version >= 5) {
        cksum = crc64(cksum, buf, len);
    }

    return len;
}

static void
rdb_set_int_env(lua_State *L, char *key, int value)
{
    lua_getglobal(L, RDB_ENV);
    script_pushtableinteger(L, key, value);
    lua_pop(L,-1);
}

// ================================== READ DATA FROOM RDB FILE. ==================================== //
static void 
rdb_check_crc(int fd, uint64_t real_crc)
{
    uint64_t expected_crc = 0;

    read(fd, &expected_crc, 8);
    memrev64ifbe(expected_crc);
    if(real_crc != expected_crc) {
        logger(ERROR, "checksum error, expect %llu, real %llu.\n", real_crc, expected_crc);
    }
}

static uint8_t
rdb_read_kv_type(int fd)
{
    char buf[1];
    if(rdb_crc_read(fd, buf, 1) == 0) {
        logger(ERROR, "Exited, as read error on load type.\n");
    }

    return (uint8_t) buf[0];
}

static uint32_t
rdb_read_store_len(int fd, uint8_t *is_encoded)
{
    char buf[2];
    uint8_t type;
    uint32_t len;

   if (is_encoded) *is_encoded = 0;
    if (rdb_crc_read(fd, buf, 1) == 0) return REDIS_RDB_LENERR;
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
        if (rdb_crc_read(fd, buf + 1, 1) == 0) return REDIS_RDB_LENERR;
        return (((uint8_t)buf[0] & 0x3f) << 8)| (uint8_t)buf[1];
    } else if (REDIS_RDB_32B == type) {
        if (rdb_crc_read(fd, &len, 4) == 0) return REDIS_RDB_LENERR;
        return ntohl(len); 
    } else {
        if(is_encoded) *is_encoded = 1;
        return (uint8_t)buf[0] & 0x3f;
    }
}

static int32_t
rdb_read_int(int fd, uint8_t enc)
{
    char buf[4];

    if (REDIS_RDB_ENC_INT8 == enc) {
        if (rdb_crc_read(fd, buf, 1) == 0) goto READERR;
        return (int8_t)buf[0];
    } else if (REDIS_RDB_ENC_INT16 == enc) {
        if (rdb_crc_read(fd, buf, 2) == 0) goto READERR;
        return (int16_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8));
    } else {
        if (rdb_crc_read(fd, buf, 4) == 0) goto READERR;
        return (int32_t)((uint8_t)buf[0] | ((uint8_t)buf[1] << 8) | ((uint8_t)buf[2] << 16) | ((uint8_t)buf[3] << 24));
    }

READERR:
    logger(ERROR, "Exited, as read error on load integer.\n");
    exit(1);
}

static char *
rdb_read_lzf_string(int fd)
{
    int32_t clen, len;
    char *cstr, *str;
    /*
     * 1. load compress length.
     * 2. load raw length.
     * 3. load lzf_string, and use lzf_decompress to decode.
     */

    if((clen = rdb_read_store_len(fd, NULL)) == REDIS_RDB_LENERR)
        return NULL;
    if((len = rdb_read_store_len(fd, NULL)) == REDIS_RDB_LENERR)
        return NULL;

    cstr = malloc(clen);
    str = malloc(len+1);
    if (!cstr || !str) {
        logger(ERROR, "Exited, as malloc failed at load lzf string.\n");
        exit(1);
    }

    int ret;
    if ((ret = rdb_crc_read(fd, cstr, clen)) == 0) goto err;
    if( (ret = lzf_decompress(cstr, clen, str, len)) == 0) goto err;
    str[len] = '\0';

    free(cstr);
    return str;

err:
    free(cstr);
    free(str);
    return NULL;
}

static char *
rdb_read_string(int fd)
{
    uint8_t is_encoded;
    uint32_t len;
    char *buf;
    int32_t val;

    len = rdb_read_store_len(fd, &is_encoded);
    if (is_encoded) {
        switch (len) {

            case REDIS_RDB_ENC_INT8:
            case REDIS_RDB_ENC_INT16:
            case REDIS_RDB_ENC_INT32:
                val = rdb_read_int(fd, len);
                return ll2string(val);

            case REDIS_RDB_ENC_LZF:
                return rdb_read_lzf_string(fd);

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
    while ( i < len && (bytes = rdb_crc_read(fd, buf + i, len - i))) {
        i += bytes;
    }
    buf[len] = '\0';

    return buf;
}

// ================================== SCRIPT PUSH UTIL. ==================================== //

static void
rdb_push_list (lua_State *L, int fd)
{
    int i, len;
    char *elem;

    len = rdb_read_store_len(fd, NULL); 
    for (i = 0; i < len; i++) {
        elem = rdb_read_string(fd);
        script_push_list_elem(L, elem, i);
        free(elem);
    }
}

static void
rdb_push_hash (lua_State *L, int fd)
{
    int i, len;
    char *key , *val;

    len = rdb_read_store_len(fd, NULL); 
    for (i = 0; i < len; i++) {
        key = rdb_read_string(fd);
        val = rdb_read_string(fd);
        script_pushtablestring(L, key, val);
        free(key);
        free(val);
    }
}

// ================================== RDB LOAD. ==================================== //

static void
rdb_set_value_type(lua_State *L, int type)
{
    char *val_type = NULL;
    switch (type) {
        case REDIS_RDB_STRING:       val_type = "string"; break;
        case REDIS_RDB_INTSET:       val_type = "set";    break;
        case REDIS_RDB_LIST_ZIPLIST: val_type = "list";   break;
        case REDIS_RDB_ZIPMAP:       val_type = "hash";   break;
        case REDIS_RDB_ZSET_ZIPLIST: val_type = "zset";   break;
        case REDIS_RDB_HASH_ZIPLIST: val_type = "hash";   break;
        case REDIS_RDB_LIST:         val_type = "list";   break;
        case REDIS_RDB_SET:          val_type = "set";    break;
        case REDIS_RDB_HASH:         val_type = "hash";   break;
        case REDIS_RDB_ZSET:         val_type = "zset";   break;
        default: return;
    }

    script_pushtablestring(L, TYPE_FIELD_STR, val_type);
}

static void
rdb_load_str_value(lua_State *L, int fd)
{
    char *str;

    str = rdb_read_string(fd);
    script_pushtablestring(L, VAL_FIELD_STR, str);

    free(str);
}

static void
rdb_load_intset_value(lua_State *L, int fd)
{
    int i;
    int64_t v64;
    char *str, *s64; 

    str = rdb_read_string(fd);
    intset *is = (intset*) str;

    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    for (i = 0; i< is->length; i++) {
        intset_get(is, i, &v64);
        s64 = ll2string(v64); 
        script_push_list_elem(L, s64, i);
        free(s64);
    }
    lua_settable(L,-3);

    free(str);
}

static void
rdb_load_zllist_value(lua_State *L, int fd)
{
    char *str;

    str = rdb_read_string(fd);
    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    push_ziplist_list_or_set(L, str);
    lua_settable(L,-3);

    free(str);
}

static void
rdb_load_zipmap_value (lua_State *L, int fd)
{
    char *str;

    str = rdb_read_string(fd);
    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    push_zipmap(L, str);
    lua_settable(L,-3);

    free(str);
}

static void
rdb_load_ziplist_value(lua_State *L, int fd)
{
    char *str;

    str = rdb_read_string(fd);
    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    push_ziplist_hash_or_zset(L, str);
    lua_settable(L,-3);

    free(str);
}

static void
rdb_load_list_or_set_value(lua_State *L, int fd)
{
    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    rdb_push_list(L, fd);
    lua_settable(L,-3);
}

static void
rdb_load_hash_or_zset_value (lua_State *L, int fd)
{
    lua_pushstring(L, VAL_FIELD_STR);
    lua_newtable(L);
    rdb_push_hash(L, fd);
    lua_settable(L,-3);
}

void
rdb_load_value(lua_State *L, int fd, int type)
{
    switch (type) {
        case       REDIS_RDB_STRING: rdb_load_str_value(L, fd); break;
        case       REDIS_RDB_INTSET: rdb_load_intset_value(L, fd); break;
        case REDIS_RDB_LIST_ZIPLIST: rdb_load_zllist_value(L, fd); break;
        case       REDIS_RDB_ZIPMAP: rdb_load_zipmap_value(L, fd); break;
        case REDIS_RDB_ZSET_ZIPLIST:
        case REDIS_RDB_HASH_ZIPLIST: rdb_load_ziplist_value(L, fd); break;
        case         REDIS_RDB_LIST:
        case          REDIS_RDB_SET: rdb_load_list_or_set_value(L, fd); break;
        case         REDIS_RDB_HASH:
        case         REDIS_RDB_ZSET: rdb_load_hash_or_zset_value(L, fd); break;

        default: logger(ERROR, "Value type error."); break;
    }
}

static uint32_t
rdb_load_time(int fd)
{
    char buf[4];
    uint32_t t32;

    if( rdb_crc_read(fd, buf, 4) == 0) {
        logger(ERROR, "Exited, as read error on load time.\n");
    }
    memcpy(&t32, buf, 4);

    return t32;
}

static uint64_t
rdb_load_ms_time(int fd)
{
    char buf[8];
    uint64_t t64;

    if( rdb_crc_read(fd, buf, 8) == 0) {
        logger(ERROR, "Exited, as read error on load microtime.\n");
    }
    memcpy(&t64, buf, 8);

    return t64;
}

int
rdb_load(lua_State *L, const char *path)
{
    char buf[128];
    uint8_t type;
    int expire_time;

    int rdb_fd = open(path, O_RDONLY);
    // read magic string(5bytes) and version(4bytes)
    if(rdb_crc_read(rdb_fd, buf, 9) == 0) {
        logger(ERROR,"Exited, as read error on laod version\n");
    }
    buf[9] = '\0';
    if(memcmp(buf, MAGIC_STR, 5) != 0) return -2;
    version = atoi(buf + 5);
    rdb_set_int_env(L, VERSION_STR, version);

    while (1) {
        expire_time = -1;
        type = rdb_read_kv_type(rdb_fd);

        // load expire time if exists
        if (REDIS_EXPIRE_SEC == type) {
            expire_time = rdb_load_time(rdb_fd); 
            type = rdb_read_kv_type(rdb_fd);
        } else if (REDIS_EXPIRE_MS == type) {
            expire_time = rdb_load_ms_time(rdb_fd) / 1000; 
            type = rdb_read_kv_type(rdb_fd);
        }

        // select db
        if (REDIS_SELECT_DB == type) {
            if (rdb_crc_read(rdb_fd, buf, 1) == 0) {
                logger(ERROR, "Exited, as read error on laod db num.\n");
            }

            rdb_set_int_env(L, DB_NUM_STR, (uint8_t)buf[0]);
            continue;
        }

        // end of rdb file
        if(REDIS_EOF == type) break;

        // read key
        char *key = rdb_read_string(rdb_fd);
        if (key) {
            lua_getglobal(L, RDB_CB); 
            lua_newtable(L);
            script_pushtablestring(L, KEY_FIELD_STR, key);
            script_pushtableinteger(L, EXP_FIELD_STR, expire_time);

            // read value
            rdb_load_value(L, rdb_fd, type);
            // set value type
            rdb_set_value_type(L, type);
            if( lua_pcall(L, 1, 0, 0) != 0 ) {
                logger(ERROR, "Runing handle function failed: %s", lua_tostring(L, -1));
            }
                
        }

        free(key);
    }

    if (version > 5) rdb_check_crc(rdb_fd, cksum);

    close(rdb_fd);

    return 0;
}
