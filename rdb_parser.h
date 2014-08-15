#ifndef __RDB_PARSER_H_
#define __RDB_PARSER_H_
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <netinet/in.h>
#include "zmalloc.h"
#include "sds.h"
#include "adlist.h"
#include "ziplist.h"
#include "intset.h"
#include "fmacros.h"
#include "lzf.h"


#define _FILE_OFFSET_BITS 64
#define PARSE_ERR -1
#define PARSE_OK 0

#define REDIS_EXPIRETIME 253
#define REDIS_SELECTDB 254
#define REDIS_EOF 255

#define REDIS_RDB_6BITLEN 0
#define REDIS_RDB_14BITLEN 1
#define REDIS_RDB_32BITLEN 2
#define REDIS_RDB_ENCVAL 3
#define REDIS_RDB_LENERR UINT_MAX

#define REDIS_RDB_ENC_INT8 0        /* 8 bit signed integer */
#define REDIS_RDB_ENC_INT16 1       /* 16 bit signed integer */
#define REDIS_RDB_ENC_INT32 2       /* 32 bit signed integer */
#define REDIS_RDB_ENC_LZF 3         /* string compressed with FASTLZ */

#define REDIS_EXPIRETIME 253            
#define REDIS_SELECTDB 254
#define REDIS_EOF 255

/* Object types */
#define REDIS_STRING 0
#define REDIS_LIST 1
#define REDIS_SET 2
#define REDIS_ZSET 3
#define REDIS_HASH 4
#define REDIS_LSET 5

#define REDIS_HASH_ZIPMAP 9
#define REDIS_LIST_ZIPLIST 10
#define REDIS_SET_INTSET 11
#define REDIS_ZSET_ZIPLIST 12

#define REDIS_ENCODING_RAW 0     /* Raw representation */
#define REDIS_ENCODING_INT 1     /* Encoded as integer */
#define REDIS_ENCODING_HT 2      /* Encoded as hash table */
#define REDIS_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define REDIS_ENCODING_LINKEDLIST 4 /* Encoded as regular linked list */
#define REDIS_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define REDIS_ENCODING_INTSET 6  /* Encoded as intset */
#define REDIS_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define REDIS_ENCODING_LONGSET 8  /* Encoded as longset */

#define REDIS_HEAD 0
#define REDIS_TAIL 1

#define parsePanic(_e) _parsePanic(#_e,__FILE__,__LINE__),_exit(1)

void _parsePanic(char *msg, char *file, int line) {
    fprintf(stderr,"!!! Software Failure. Press left mouse button to continue");
    fprintf(stderr,"Guru Meditation: %s #%s:%d",msg,file,line);
#ifdef HAVE_BACKTRACE
    fprintf(stderr,"(forcing SIGSEGV in order to print the stack trace)");
    *((char*)-1) = 'x';
#endif
}

typedef struct {
    off_t total_bytes;
    off_t parsed_bytes;
    time_t start_time;
} parserStats;

typedef struct {
    unsigned char *subject;
    unsigned char encoding;
    unsigned char direction; /* Iteration direction */
    unsigned char *zi; 
} listTypeIterator;

/* Structure for an entry while iterating over a list. */
typedef struct {
    listTypeIterator *li;
    unsigned char *zi;  /* Entry in ziplist */
} listTypeEntry;

/* Structure to hold set iteration abstraction. */
typedef struct {
    unsigned char *subject;
    int encoding;
    int ii; /* intset iterator */
} setTypeIterator;

typedef void (*keyValueHandler) (int type, void *key, void *val, time_t expiretime);

//keyValueHandler handler;
#endif
