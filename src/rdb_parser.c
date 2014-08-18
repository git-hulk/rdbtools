/*
 * rdb parser for redis.
 * author: @hulk 
 * date: 2014-08-13
 *
 * all value object data type we should handle below:
 * 1. string, we return like "abc" 
 * 2. list,  we return array like ["abc", "test", "resize"]
 * 3. set, is the same with list, like [1, 3, 4, 5]
 * 4. zset, we return ["member1",score1, "member2", score2...]
 * 5. hash, we return ["key1", "value1", "key2", "value2"...]
 */
#include "rdb_parser.h"
#include "lzf.h"
#include "crc64.c"
#include "util.h"
#include <stdlib.h>
#include <arpa/inet.h>

int rdb_version;
double R_Zero, R_PosInf, R_NegInf, R_Nan;
static parserStats parser_stats;
/* record check sum */
static long long digest = 0;

size_t fread_check(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t len = fread(ptr, size, nmemb, stream);
    digest = crc64(digest, (unsigned char*)ptr, size*len);
    return len; 
}

int rdbLoadType(FILE *fp) {
    unsigned char type;
    if (fread_check(&type,1,1,fp) == 0) return -1;
    return type;
}

long long rdbLoadTime(FILE *fp) {
    if (rdb_version < 5) {
        int32_t t32;
        if (fread_check(&t32,4,1,fp) == 0) return -1;
        return (long long) t32;
    } else {
        int64_t t64;
        if (fread_check(&t64,8,1,fp) == 0) return -1;
        return (long long) t64;
    }
}

/* For information about double serialization check rdbSaveDoubleValue() */
int rdbLoadDoubleValue(FILE *fp, double *val) {
    char buf[256];
    unsigned char len;

    if (fread_check(&len,1,1,fp) == 0) return -1;
    switch(len) {
        case 255: *val = R_NegInf; return 0;
        case 254: *val = R_PosInf; return 0;
        case 253: *val = R_Nan; return 0;
        default:
                  if (fread_check(buf,len,1,fp) == 0) return -1;
                  buf[len] = '\0';
                  sscanf(buf, "%lg", val);
                  return 0;
    }
}

uint32_t rdbLoadLen(FILE *fp, int *isencoded) {
    unsigned char buf[2];
    uint32_t len;
    int type;

    if (isencoded) *isencoded = 0;
    if (fread_check(buf,1,1,fp) == 0) return REDIS_RDB_LENERR;
    type = (buf[0]&0xC0)>>6;
    if (type == REDIS_RDB_6BITLEN) {
        /* Read a 6 bit len */
        return buf[0]&0x3F;
    } else if (type == REDIS_RDB_ENCVAL) {
        /* Read a 6 bit len encoding type */
        if (isencoded) *isencoded = 1;
        return buf[0]&0x3F;
    } else if (type == REDIS_RDB_14BITLEN) {
        /* Read a 14 bit len */
        if (fread_check(buf+1,1,1,fp) == 0) return REDIS_RDB_LENERR;
        return ((buf[0]&0x3F)<<8)|buf[1];
    } else {
        /* Read a 32 bit len */
        if (fread_check(&len,4,1,fp) == 0) return REDIS_RDB_LENERR;
        return ntohl(len);
    }
}

sds rdbLoadIntegerObject(FILE *fp, int enctype, int encode) {
    unsigned char enc[4];
    long long val;

    encode = -1; /* unsed */
    if (enctype == REDIS_RDB_ENC_INT8) {
        if (fread_check(enc,1,1,fp) == 0) return NULL;
        val = (signed char)enc[0];
    } else if (enctype == REDIS_RDB_ENC_INT16) {
        uint16_t v;
        if (fread_check(enc,2,1,fp) == 0) return NULL;
        v = enc[0]|(enc[1]<<8);
        val = (int16_t)v;
    } else if (enctype == REDIS_RDB_ENC_INT32) {
        uint32_t v;
        if (fread_check(enc,4,1,fp) == 0) return NULL;
        v = enc[0]|(enc[1]<<8)|(enc[2]<<16)|(enc[3]<<24);
        val = (int32_t)v;
    } else {
        val = 0; /* anti-warning */
        parsePanic("Unknown RDB integer encoding type");
    }
    return sdsfromlonglong(val);
}

sds rdbLoadLzfStringObject(FILE*fp) {
    unsigned int len, clen;
    unsigned char *c = NULL;
    sds val = NULL;

    if ((clen = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
    if ((len = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
    if ((c = zmalloc(clen)) == NULL) goto err;
    if ((val = sdsnewlen(NULL,len)) == NULL) goto err;
    if (fread_check(c,clen,1,fp) == 0) goto err;
    if (lzf_decompress(c,clen,val,len) == 0) goto err;
    zfree(c);
    return val;
err:
    zfree(c);
    sdsfree(val);
    return NULL;
}

sds rdbGenericLoadStringObject(FILE*fp, int encode) {
    int isencoded;
    uint32_t len; 
    sds val; 

    len = rdbLoadLen(fp,&isencoded);
    if (isencoded) {
        switch(len) {
            case REDIS_RDB_ENC_INT8:
            case REDIS_RDB_ENC_INT16:
            case REDIS_RDB_ENC_INT32:
                return rdbLoadIntegerObject(fp,len,encode);
            case REDIS_RDB_ENC_LZF:
                return rdbLoadLzfStringObject(fp);
            default:
                parsePanic("Unknown RDB encoding type");
        }    
    }    

    if (len == REDIS_RDB_LENERR) return NULL;
    val = sdsnewlen(NULL,len);
    if (len && fread_check(val,len,1,fp) == 0) { 
        sdsfree(val);
        return NULL;
    }    
    return val; 
}

sds rdbLoadEncodedStringObject(FILE *fp) {
    return rdbGenericLoadStringObject(fp,1);
}

sds rdbLoadStringObject(FILE *fp) {
    return rdbGenericLoadStringObject(fp,0);
}

/* load value which hash encoding with zipmap. */
void *loadHashZipMapObject(unsigned char* zm, unsigned int *rlen) {
    unsigned char *key, *val, *p;
    unsigned int klen, vlen, len, i = 0;

    len = zipmapLen(zm);
    *rlen = len * 2;
    sds *results = (sds *) zmalloc(len * 2 * sizeof(sds));
    p = zipmapRewind(zm);
    while((p = zipmapNext(p,&key,&klen,&val,&vlen)) != NULL) {
        results[i] = sdsnewlen(key, klen);
        results[i+1] = sdsnewlen(val, vlen);
        i += 2;
    }
    return results;
}

void *loadListZiplistObject(unsigned char* zl, unsigned int *rlen) {
    unsigned int i = 0,len;
    listTypeIterator *li;
    listTypeEntry entry;

    len = ziplistLen(zl);
    *rlen = len;
    li = listTypeInitIterator(zl,0,REDIS_TAIL);
    sds *results = (sds *) zmalloc(len * sizeof(sds));
    while (listTypeNext(li,&entry)) {
        results[i++] = listTypeGet(&entry); 
    }
    listTypeReleaseIterator(li);

    return results;
}

void* loadSetIntsetObject(unsigned char* sl, unsigned int *rlen) {
    int64_t intele;
    unsigned int i = 0, len;

    setTypeIterator *si;
    len = ((intset*)sl)->length;
    *rlen = len;
    si = setTypeInitIterator(sl); 

    sds *results = zmalloc(len * sizeof(sds));
    while (setTypeNext(si,&intele) != -1) {
        results[i++] = sdsfromlonglong(intele); 
    }
    setTypeReleaseIterator(si);

    return results;
}

void *loadZsetZiplistObject(unsigned char* zl, unsigned int *rlen) {
    unsigned int i = 0, len;
    unsigned char *eptr, *sptr;
    unsigned char *vstr;
    unsigned int vlen;
    int buf_len;
    long long vlong;
    double score;
    char buf[128];
    sds ele;

    len = ziplistLen (zl);
    eptr = ziplistIndex(zl,0);
    sptr = ziplistNext(zl,eptr);
    sds *results = (sds *) zmalloc(len* 2 * sizeof(sds));
    if(rdb_version < 2) {
        *rlen = len * 2;
    } else {
        *rlen = len;
    }
    while (eptr != NULL) {
        ziplistGet(eptr,&vstr,&vlen,&vlong);
        if (vstr == NULL)
            ele = sdsfromlonglong(vlong);
        else
            ele = sdsnewlen((char*)vstr,vlen);
        results[i] = ele;
        buf_len = snprintf(buf, 128, "%f", score);
        results[i+1] = sdsnewlen(buf, buf_len);
        i += 2;
        score = zzlGetScore(sptr);
        zzlNext(zl,&eptr,&sptr);
    }

    return results;
}

void* rdbLoadValueObject(FILE *fp, int type, unsigned int *rlen) {
    unsigned int i, j, len;
    int buf_len;
    sds ele;
    sds *results;
    char buf[128];

    if(type == REDIS_STRING) {
        /* value type is string. */
        parser_stats.parse_num[STRING] += 1;
        ele = rdbLoadEncodedStringObject(fp);
        *rlen = sdslen(ele);
        return ele;

    } else if(type == REDIS_LIST) {
        /* value type is list. */
        parser_stats.parse_num[LIST] += 1;
        if ((len = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
        j = 0;
        *rlen = len;
        results = zmalloc(len * sizeof(*results));
        while(len--) {
            if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return NULL;
            results[j++] = ele;
        }
        return results;

    } else if(type == REDIS_SET) {
        /* value type is set. */
        parser_stats.parse_num[SET] += 1;
        if ((len = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
        *rlen = len;
        results = zmalloc(len * sizeof(*results));
        for (i = 0; i < len; i++) {
            if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return NULL;
            results[i] = ele; 
        }
        return results;

    } else if (type == REDIS_ZSET) {
        /* value type is zset */
        parser_stats.parse_num[ZSET] += 1;
        size_t zsetlen;
        double score;
        j = 0;    
        if ((zsetlen = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
        if(rdb_version < 2) {
            *rlen = zsetlen * 2;
        } else {
            *rlen = zsetlen;
        }
        results = zmalloc( zsetlen * 2 * sizeof(*results));
        while(zsetlen--) {
            if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return NULL;
            if (rdbLoadDoubleValue(fp,&score) == -1) return NULL;
            buf_len = snprintf(buf, 128, "%f", score);
            results[j] = ele;
            results[j+1] = sdsnewlen(buf, buf_len);
            j += 2;
        }
        return results;

    } else if (type == REDIS_HASH) {
        /* value type is hash */
        parser_stats.parse_num[HASH] += 1;
        size_t hashlen;
        if ((hashlen = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return NULL;
        sds key, val;
        j = 0;
        *rlen = hashlen * 2;
        results = zmalloc(hashlen * 2 * sizeof(*results));
        while(hashlen--) {
            if ((key = rdbLoadEncodedStringObject(fp)) == NULL) return NULL;
            if ((val = rdbLoadEncodedStringObject(fp)) == NULL) return NULL;
            results[j] = key;
            results[j + 1] = val;
            j += 2;
        }
        return results;

    } else if(type == REDIS_HASH_ZIPMAP || 
            type == REDIS_LIST_ZIPLIST ||
            type == REDIS_SET_INTSET ||
            type == REDIS_ZSET_ZIPLIST ||
            type == REDIS_LSET) {
        sds aux = rdbLoadStringObject(fp); 
        switch(type) {
            case REDIS_HASH_ZIPMAP:
                parser_stats.parse_num[HASH] += 1;
                return loadHashZipMapObject((unsigned char*)aux, rlen);
            case REDIS_LIST_ZIPLIST:
                parser_stats.parse_num[LIST] += 1;
                return loadListZiplistObject((unsigned char *)aux, rlen);
            case REDIS_SET_INTSET:
                parser_stats.parse_num[SET] += 1;
                return loadSetIntsetObject((unsigned char *)aux, rlen);
            case REDIS_ZSET_ZIPLIST:
                parser_stats.parse_num[ZSET] += 1;
                return loadZsetZiplistObject((unsigned char *)aux, rlen);
        }
        sdsfree(aux);

        return NULL;
    } else {
        parsePanic("Unknown object type");
    }
}

void start_parse(FILE *fp) {
    int i;
    struct stat sb;
    parser_stats.start_time = time(NULL);
    parser_stats.parsed_bytes = 0;
    if (fstat(fileno(fp), &sb) == -1) {
        parser_stats.total_bytes = 1;
    } else {
        parser_stats.total_bytes = sb.st_size;
    }
    for(i = 0; i < TOTAL_DATA_TYPES; i++) {
        parser_stats.parse_num[i] = 0;
    }
}

void parse_progress(off_t pos) {
    parser_stats.parsed_bytes = pos;
}

int rdb_parse(char *rdbFile, keyValueHandler handler) {
    int type, loops = 0, dbid, valType;
    unsigned int rlen;
    char buf[1024];
    time_t expiretime = -1;
    FILE *fp;
    sds key, sval; /* sval is simple string value.*/
    sds *cval; /*complicatae value*/

    /* Double constants initialization */
    R_Zero = 0.0;
    R_PosInf = 1.0/R_Zero;
    R_NegInf = -1.0/R_Zero;
    R_Nan = R_Zero/R_Zero;

    if((fp = fopen(rdbFile, "r")) == NULL) {
        return PARSE_ERR;
    }

    if (fread_check(buf, 9, 1, fp) == 0) {
        fclose(fp);
        fprintf(stderr, "fread err :%s\n", strerror(errno));
        return PARSE_ERR;
    }
    buf[9] = '\0';
    if (memcmp(buf, "REDIS", 5) != 0) {
        fclose(fp);
        fprintf(stderr, "Wrong signature trying to load DB from file\n");
        return PARSE_ERR;
    }
    rdb_version = atoi(buf+5);
    if (rdb_version > 6) {
        fclose(fp);
        fprintf(stderr, "Can't handle RDB format version %d\n", rdb_version);
        return PARSE_ERR;
    }

    start_parse(fp);
    while(1) {
        if(!(loops++ % 1000)) {
            /* record parse progress every 1000 loops. */
            parse_progress(ftello(fp));
        }
        if((type = rdbLoadType(fp)) == -1) return PARSE_ERR;

        if(type == REDIS_EXPIRETIME) {
            if ((expiretime = rdbLoadTime(fp)) == -1) return PARSE_ERR;
            if((type = rdbLoadType(fp)) == -1) return PARSE_ERR;
        }
        /* file end. */
        if(type == REDIS_EOF) {
            break;
        }
        /* select db */
        if(type == REDIS_SELECTDB) {
            dbid = rdbLoadLen(fp,NULL);
            continue;
        }

        /* load key. */
        if ((key = rdbLoadStringObject(fp)) == NULL) {
            return PARSE_ERR;
        } 

        if(type == REDIS_HASH_ZIPMAP) {
            valType = REDIS_HASH;
        } else if(type == REDIS_LIST_ZIPLIST) {
            valType = REDIS_LIST;
        } else if(type == REDIS_SET_INTSET) {
            valType = REDIS_SET;
        } else if(type == REDIS_ZSET_ZIPLIST) {
            valType = REDIS_ZSET;
        } else {
            valType = type;
        }
        /* load value. */
        if(type == REDIS_STRING) {
            sval = rdbLoadValueObject(fp, type, &rlen);
            handler(valType, key, sval, rlen, expiretime);
        } else {
            cval = rdbLoadValueObject(fp, type, &rlen);
            handler(valType, key, cval, rlen, expiretime);
        }

        /* clean*/
        sdsfree(key);
        if(valType == REDIS_STRING) {
            sdsfree(sval);
        } else {
            unsigned int k;
            for(k = 0; k < rlen; k++) {
                sdsfree(cval[k]);
            }
            zfree(cval);
        }
    }

    /* checksum */
    uint64_t checksum = 0;
    int ret;
    ret = fread(&checksum, sizeof(checksum), 1, fp);
    if (ret == 1) {
        if ((long long)checksum != digest) {
            fprintf(stderr, "DB load failed, checksum does not match: %016llx != %016llx\n", (long long)checksum, digest);
            exit(1);
        }
        fprintf(stderr, "DB loaded, checksum: %016llx\n", digest);
    }
    parser_stats.stop_time = time(NULL);
    fclose(fp);

    return PARSE_OK;
}

void dumpParserInfo() {
    long long total_nums = 0;
    int i;
    for(i = 0 ; i < TOTAL_DATA_TYPES; i++) {
        total_nums += parser_stats.parse_num[i];
    }

    printf("--------------------------------------------DUMP INFO------------------------------------------\n");
    printf("Parser parse %ld bytes  cost %ds.\n", (long) parser_stats.total_bytes, (int)((int)parser_stats.stop_time - (int)parser_stats.start_time));
    printf("Total parse %lld keys\n", total_nums);
    printf("\t%ld String keys\n", parser_stats.parse_num[STRING]);
    printf("\t%ld List keys\n", parser_stats.parse_num[LIST]);
    printf("\t%ld Set keys\n", parser_stats.parse_num[SET]);
    printf("\t%ld Zset keys\n", parser_stats.parse_num[ZSET]);
    printf("\t%ld Hash keys\n", parser_stats.parse_num[HASH]);
    printf("--------------------------------------------DUMP INFO------------------------------------------\n");
}

