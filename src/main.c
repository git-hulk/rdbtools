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

void userHandler (int type, void *key, void *val, unsigned int vlen, time_t expiretime) {
    unsigned int i;
    if(type == REDIS_STRING) {
        printf("STRING\t%d\t%s\t%s\n", (int)expiretime, (char *)key, (char *)val);
    } else if (type == REDIS_SET) {
        sds *res = (sds *)val;
        printf("SET\t%d\t%s\t[", (int)expiretime, (char *)key);
        for(i = 0; i < vlen; i++) {
            printf("%s, ", res[i]);
        }
        printf("]\n");
    } else if(type == REDIS_LIST) {
        sds *res = (sds *)val;
        printf("LIST\t%d\t%s\t[", (int)expiretime, (char *)key);
        for(i = 0; i < vlen; i++) {
            printf("%s, ", res[i]);
        }
        printf("]\n");
    } else if(type == REDIS_ZSET) { 
        sds *res = (sds *)val;
        printf("ZSET\t%d\t%s\t", (int)expiretime, (char *)key);
        for(i = 0; i < vlen; i += 2) {
            printf("(%s,", res[i]);
            printf("%s), ", res[i+1]);
        }
        printf("\n");

    } else if(type == REDIS_HASH) {
        sds *res = (sds *)val;
        printf("LIST\t%d\t%s\t", (int)expiretime, (char *)key);
        for(i = 0; i < vlen; i += 2) {
            printf("(%s, ", res[i]);
            printf("%s), ", res[i+1]);
        }
        printf("\n");
    }
}


int main(int argc, char **argv) {

    if(argc <= 1) {
        fprintf(stderr, "Usage:\nrdb_parser -[f file]\n\t-f --file specify which rdb file would be parsed.\n");
        exit(1);
    }

    int i;
    BOOL dumpParseInfo = FALSE;
    char *rdbFile;
    for(i = 1; i < argc; i++) {
        if(argc > i+1 && argv[i][0] == '-' && argv[i][1] == 'f') {
            i += 1;
            rdbFile = argv[i];
        } else if(argv[i][0] == '-' && argv[i][1] == 'd'){
            dumpParseInfo = TRUE;
        }
    }
    if(!rdbFile) {
        fprintf(stderr, "ERR: U must specify rdb file by option -f filepath.\n");
        exit(1);
    }

    /* start parse rdb file. */
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    rdb_parse(rdbFile, userHandler);
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    if(dumpParseInfo) {
        dumpParserInfo(); 
    }
    return 0;
}
