#ifndef __RDB_PARSER_H_
#define __RDB_PARSER_H_
#include "main.h"

typedef struct {
    off_t total_bytes;
    off_t parsed_bytes;
    time_t start_time;
} parserStats;
 typedef void keyValueHandler (int type, void *key, void *val,unsigned int vlen,time_t expiretime);

int rdb_parse(char *rdbFile, keyValueHandler handler);
#endif
