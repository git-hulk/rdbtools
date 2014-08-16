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

typedef void keyValueHandler (int type, void *key, void *val,unsigned int vlen,time_t expiretime);
void dumpParserInfo();
int rdb_parse(char *rdbFile, keyValueHandler handler);

#endif
