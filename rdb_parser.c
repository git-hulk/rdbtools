/*
 * rdb parser for redis.
 * author: @hulk 
 * date: 2014-08-13
 */
#include "rdb_parser.h"
#include <stdlib.h>
#include "crc64.c"

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

void rdbLoadValueObject(FILE *fp, int type) {
    unsigned int i;
    size_t len;
    sds ele;

    if(type == REDIS_STRING) {
        /* value type is string. */
        rdbLoadEncodedStringObject(fp);
    } else if(type == REDIS_LIST) {
        /* value type is list. */
        if ((len = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return;
        while(len--) {
            if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return;
        }
    } else if(type == REDIS_SET) {
        /* value type is set. */
        if ((len = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return;
        for (i = 0; i < len; i++) {
            if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return;
        }
    } else if (type == REDIS_ZSET) {
        /* value type is zset */
        size_t zsetlen;
        double score;
         if ((zsetlen = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return;
         while(zsetlen--) {
             if ((ele = rdbLoadEncodedStringObject(fp)) == NULL) return;
             if (rdbLoadDoubleValue(fp,&score) == -1) return;
         }
    } else if (type == REDIS_HASH) {
        /* value type is hash */
        size_t hashlen;
        if ((hashlen = rdbLoadLen(fp,NULL)) == REDIS_RDB_LENERR) return;
        sds key, val;
        while(hashlen--) {
            if ((key = rdbLoadEncodedStringObject(fp)) == NULL) return;
            if ((val = rdbLoadEncodedStringObject(fp)) == NULL) return;
        }
    } else if(type == REDIS_HASH_ZIPMAP || 
            type == REDIS_LIST_ZIPLIST ||
            type == REDIS_SET_INTSET ||
            type == REDIS_ZSET_ZIPLIST ||
            type == REDIS_LSET) {
       sds aux = rdbLoadStringObject(fp); 
       switch(type) {
           case REDIS_HASH_ZIPMAP:
               
       }

    } else {
        parsePanic("Unknown object type");
    }
}

void start_parse(FILE *fp) {
    struct stat sb;
    parser_stats.start_time = time(NULL);
    parser_stats.parsed_bytes = 0;
    if (fstat(ftello(fp), &sb) == -1) {
        parser_stats.total_bytes = 1;
    } else {
        parser_stats.total_bytes = sb.st_size;
    }
}

void parse_progress(off_t pos) {
    parser_stats.parsed_bytes = pos;
}

int rdb_parse(char *rdbFile) {
    int type, loops = 0, dbid;
    char buf[1024];
    time_t expiretime;
    FILE *fp;
    sds key;

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
    if (rdb_version < 3 || rdb_version > 6) {
        fclose(fp);
        fprintf(stderr, "Can't handle RDB format version %d\n", rdb_version);
        return PARSE_ERR;
    }
    /**
      if (buf[8] != 'c') {
      fclose(fp);
      return PARSE_ERR;
      }
     **/

    printf("-----------------Start Parse---------------------\n");

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
        /* load value. */
        rdbLoadValueObject(fp, type);

        /*free key.*/
        sdsfree(key);
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

    printf("-----------------Stop  Parse---------------------\n");
    fclose(fp);

    return PARSE_OK;
}

int main(int argc, char **argv) {

    if(argc <= 1) {
        fprintf(stderr, "Usage:\nrdb_parser -[f file]\n\t-f --file specify which rdb file would be parsed.\n");
        exit(1);
    }

    int i;
    char *rdbFile;
    for(i = 1; i < argc; i++) {
        if(argc > i+1 && argv[i][0] == '-' && argv[i][1] == 'f') {
            i += 1;
            rdbFile = argv[i];
        }
    }
    if(!rdbFile) {
        fprintf(stderr, "ERR: U must specify rdb file by option -f filepath.\n");
        exit(1);
    }

    /* start parse rdb file. */
    rdb_parse(rdbFile);
    return 0;
}
