/*
 * rdb parser for redis.
 * author: @hulk 
 * date: 2014-08-13
 */
#include <getopt.h>
#include "main.h"
#include "rdb_parser.h"
#include "callbacks.c"

static void usage(void) {
    fprintf(stderr, "USAGE: ./rtools [-f file] -V -h\n");
    fprintf(stderr, "\t-V --version \n");
    fprintf(stderr, "\t-h --help show usage \n");
    fprintf(stderr, "\t-f --file specify which rdb file would be parsed.\n");
    fprintf(stderr, "\t Notice: This tool only test on redis 2.2 and 2.4, 2.6, 2.8.\n\n");
}

int main(int argc, char **argv) {
    int c;
    int parse_result;
    char *rdbFile;
    bool is_show_help = false, is_show_version = false;
    char short_options [] = { "hVf:" };
    struct option long_options[] = {
         { "help", no_argument, NULL, 'h' }, /* help */
         { "version", no_argument, NULL, 'V' }, /* version */
         { "rdb-iile-path", required_argument,  NULL, 'f' }, /* rdb file path*/
         { NULL, 0, NULL, 0  }
    };

    for (;;) {
        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }
        switch (c) {
            case 'h':
                is_show_help = true;
                break;
            case 'V':
                is_show_version = true;
                break;
            case 'f':
                rdbFile = optarg;
                break;
            default:
                exit(0);
        }
    }

    if(is_show_version) {
        fprintf(stderr, "\nHELLO, THIS RDB PARSER VERSION 1.0\n\n");
    }
    if(is_show_help) {
        usage();
    }
    if(is_show_version || is_show_help) {
        exit(0);
    }
    if(!rdbFile) {
        fprintf(stderr, "ERR: U must specify rdb file by option -f filepath.\n");
        exit(0);
    }

    /* start parse rdb file. */
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    parse_result = rdbParse(rdbFile, userHandler);
    printf("--------------------------------------------RDB PARSER------------------------------------------\n");
    dumpParserInfo(); 
    return 0;
}
