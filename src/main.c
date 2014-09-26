/*
 * rdb parser for redis.
 * author: @hulk 
 * date: 2014-08-13
 */
#include "main.h"
#include "rdb_parser.h"

void _parsePanic(char *msg, char *file, int line) {
    fprintf(stderr,"!!! Software Failure. Press left mouse button to continue");
    fprintf(stderr,"Guru Meditation: %s #%s:%d",msg,file,line);
#ifdef HAVE_BACKTRACE
    fprintf(stderr,"(forcing SIGSEGV in order to print the stack trace)");
    *((char*)-1) = 'x';
#endif
}

/**
 * when a key was parsed, this function would be called once.
 *
 * @type: all are listed below example.
 * @key: key in redis db.
 * @val & @vlen: 
 * if type = REDIS_STRING, val is string, vlen is length of string.
 * if type = REDIS_SET, val is string array, vlen is num of elems in val array, 
 *     like ["1", "2", "3"], vlen=3  
 * if type = REDIS_LIST, result is same with REDIS_SET
 * if type = REDIS_ZSET, val is string array, even index is elem, odd index is score
 *     like ["elem1", 1.9, "elem2", 2.1], vlen = 4
 * if type = REDIS_HASH, val is string array, even index is field, odd index is value 
 *     like ["field1", "val1", "field2", "val2"], vlen = 4
 *
 * @expiretime: unix time in micro second. 
 *
 * Note: don't free key, val. As rdbtools would free them after call userHandler.
 */
void* userHandler (int type, void *key, void *val, unsigned int vlen, long long expiretime) {

    /* I didn't use these vars in example, so mark as unused */
    V_NOT_USED(key);
    V_NOT_USED(val);
    V_NOT_USED(vlen);
    V_NOT_USED(expiretime);

    if(type == REDIS_STRING) {
        /* handle whici type is string */
    } else if (type == REDIS_SET) {
        /* handle whici type is set */
    } else if(type == REDIS_LIST) {
        /* handle whici type is list */
    } else if(type == REDIS_ZSET) { 
        /* handle whici type is zset */
    } else if(type == REDIS_HASH) {
        /* handle whici type is hash */
    }
    return NULL;
}


static void usage(void) {
    fprintf(stderr, "Usage:\nrdb_parser -[f file] [d]\n");
    fprintf(stderr, "\t-f --file specify which rdb file would be parsed.\n");
    fprintf(stderr, "\t-d --dump parser info, to dump parser stats info.\n");
    fprintf(stderr, "\t Notice: This tool only test on redis 2.2 and 2.4, so it may be error in 2.4 later.\n");
}

int main(int argc, char **argv) {

    if(argc <= 1) {
        usage();
        exit(1);
    }

    int i, parse_result;
    bool dumpParseInfo = false;
    char *rdbFile;
    for(i = 1; i < argc; i++) {
        if(argc > i+1 && argv[i][0] == '-' && argv[i][1] == 'f') {
            i += 1;
            rdbFile = argv[i];
        } else if(argv[i][0] == '-' && argv[i][1] == 'd'){
            dumpParseInfo = true;
        }
    }
    if(!rdbFile) {
        fprintf(stderr, "ERR: U must specify rdb file by option -f filepath.\n");
        exit(1);
    }

    /* start parse rdb file. */
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    parse_result = rdbParse(rdbFile, userHandler);
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    if(parse_result == PARSE_OK && dumpParseInfo) {
        dumpParserInfo(); 
    }
    return 0;
}
