#include "fmacros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <sys/time.h>
#include "main.h"

/* Convert a long long into a string. Returns the number of
 * characters needed to represent the number, that can be shorter if passed
 * buffer length is not enough to store the whole number. */
int ll2string(char *s, size_t len, long long value) {
    char buf[32], *p;
    unsigned long long v;
    size_t l;

    if (len == 0) return 0;
    v = (value < 0) ? -value : value;
    p = buf+31; /* point to the last character */
    do {
        *p-- = '0'+(v%10);
        v /= 10;
    } while(v);
    if (value < 0) *p-- = '-';
    p++;
    l = 32-(p-buf);
    if (l+1 > len) l = len-1; /* Make sure it fits, including the nul term */
    memcpy(s,p,l);
    s[l] = '\0';
    return l;
}

/* Convert a string into a long long. Returns 1 if the string could be parsed
 * into a (non-overflowing) long long, 0 otherwise. The value will be set to
 * the parsed value when appropriate. */
int string2ll(char *s, size_t slen, long long *value) {
    char *p = s;
    size_t plen = 0;
    int negative = 0;
    unsigned long long v;

    if (plen == slen)
        return 0;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return 1;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return 0;
    }

    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return 1;
    } else {
        return 0;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* Overflow. */
            return 0;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0]-'0'))) /* Overflow. */
            return 0;
        v += p[0]-'0';

        p++; plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return 0;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN+1))+1)) /* Overflow. */
            return 0;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow. */
            return 0;
        if (value != NULL) *value = v;
    }
    return 1;
}

/*-----------------------------------------------------------------------------
 * List API
 *----------------------------------------------------------------------------*/

listTypeIterator *listTypeInitIterator(unsigned char *subject, int index, unsigned char direction) {
    listTypeIterator *li = zmalloc(sizeof(listTypeIterator));
    li->subject = subject;
    li->direction = direction;
    li->zi = ziplistIndex(subject,index);
    return li;
}

sds listTypeGet(listTypeEntry *entry) {
    sds value = NULL;
    unsigned char *vstr;
    unsigned int vlen;
    long long vlong;

    if (ziplistGet(entry->zi,&vstr,&vlen,&vlong)) {
        if (vstr) {
            value = sdsnewlen((char*)vstr,vlen);
        } else {
            value = sdsfromlonglong(vlong);
        }    
    }    
    return value;
}

void listTypeReleaseIterator(listTypeIterator *li) {
    zfree(li);
}

int listTypeNext(listTypeIterator *li, listTypeEntry *entry) {
    entry->li = li;
    entry->zi = li->zi;
    if (entry->zi != NULL) {
        if (li->direction == REDIS_TAIL)
            li->zi = ziplistNext(li->subject,li->zi);
        else
            li->zi = ziplistPrev(li->subject,li->zi);
        return 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
 * set API
 *----------------------------------------------------------------------------*/
setTypeIterator *setTypeInitIterator(unsigned char* subject) {
    setTypeIterator *si = zmalloc(sizeof(setTypeIterator));
    si->subject = subject;
    si->encoding = REDIS_ENCODING_INTSET;
    si->ii = 0;
    return si;
}

void setTypeReleaseIterator(setTypeIterator *si) {
    zfree(si);
}

int setTypeNext(setTypeIterator *si ,int64_t *llele) {
    if (!intsetGet((intset*)si->subject,si->ii++,llele))
        return -1;
    return si->encoding;
}

/*-----------------------------------------------------------------------------
 * Ziplist-backed sorted set API
 *----------------------------------------------------------------------------*/

double zzlGetScore(unsigned char *sptr) {
    unsigned char *vstr;
    unsigned int vlen;
    long long vlong;
    char buf[128];
    double score;

    ziplistGet(sptr,&vstr,&vlen,&vlong);
    if (vstr) {
        memcpy(buf,vstr,vlen);
        buf[vlen] = '\0';
        score = strtod(buf,NULL);
    } else {
        score = vlong;
    }

    return score;
}

void zzlNext(unsigned char *zl, unsigned char **eptr, unsigned char **sptr) {
    unsigned char *_eptr, *_sptr;

    _eptr = ziplistNext(zl,*sptr);
    if (_eptr != NULL) {
        _sptr = ziplistNext(zl,_eptr);
    } else {
        /* No next entry. */
        _sptr = NULL;
    }

    *eptr = _eptr;
    *sptr = _sptr;
}
