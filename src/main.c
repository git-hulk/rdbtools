/*
 * rdb parser for redis.
 * author: @hulk 
 * date: 2014-08-13
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "rdb.h"

static void
usage(void)
{
    fprintf(stderr, "USAGE: ./rtools [-f file] -V -h\n");
    fprintf(stderr, "\t-V --version \n");
    fprintf(stderr, "\t-h --help show usage \n");
    fprintf(stderr, "\t-f --file specify which rdb file would be parsed.\n");
    fprintf(stderr, "\t Notice: This tool only test on redis 2.2 and 2.4, 2.6, 2.8.\n\n");
}

int
main(int argc, char **argv)
{
    int c;
    char *rdb_file;
    int is_show_help = 0, is_show_version = 0;
    char short_options [] = { "hVf:" };
    lua_State *L;

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
                is_show_help = 1;
                break;
            case 'V':
                is_show_version = 1;
                break;
            case 'f':
                rdb_file = optarg;
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
    if(!rdb_file) {
        fprintf(stderr, "ERR: U must specify rdb file by option -f filepath.\n");
        exit(0);
    }

    L = script_init("example.lua");
    rdb_load(L, rdb_file);
    lua_close(L);
    return 0;
}
