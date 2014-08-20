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

#ifndef __RDB_PARSER_H_
#define __RDB_PARSER_H_
#include "main.h"

#define STRING 0
#define LIST 1
#define SET 2
#define ZSET 3
#define HASH 4
#define TOTAL_DATA_TYPES 5

typedef struct {
    off_t total_bytes;
    off_t parsed_bytes;
    time_t start_time;
    time_t stop_time;
    long parse_num[TOTAL_DATA_TYPES];
} parserStats;

typedef void* keyValueHandler (int type, void *key, void *val,unsigned int vlen,time_t expiretime);
void dumpParserInfo();
int rdbParse(char *rdbFile, keyValueHandler handler);

#endif
