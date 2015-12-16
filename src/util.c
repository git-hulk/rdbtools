#include "util.h"
#include <stdio.h>
#include <stdlib.h>

char *
ll2string(long long v)
{
    int i, len = 2;
    long long tmp;
    char *buf;
    
    tmp = v;
    while ((tmp /= 10) > 0) len++;
    
    buf = malloc(len);
    if (!buf) {
        fprintf(stderr, "Exited, as malloc failed at ll2string.\n");
        exit(1);
    }

    i = len - 2;
    while (v > 0) {
        buf[i--] = v % 10 + '0';
        v /= 10;
    }
    buf[len - 1] = '\0';

    return buf;
}

#ifdef _UTIL_
int main()
{
    int v1 = 0;
    int v2 = 10;
    int v3 = 100;
    int v4 = 100123;
    printf("%d to %s\n", v1, ll2string(v1));
    printf("%d to %s\n", v2, ll2string(v2));
    printf("%d to %s\n", v3, ll2string(v3));
    printf("%d to %s\n", v4, ll2string(v4));
    return 0;
}
#endif
