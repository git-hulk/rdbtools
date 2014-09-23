#ifndef __REDIS_UTIL_H
#define __REDIS_UTIL_H
#include "main.h"

int ll2string(char *s, size_t len, long long value);
int string2ll(char *s, size_t slen, long long *value);

listTypeIterator *listTypeInitIterator(unsigned char *subject, int index, unsigned char direction);
sds listTypeGet(listTypeEntry *entry);
void listTypeReleaseIterator(listTypeIterator *li);
int listTypeNext(listTypeIterator *li, listTypeEntry *entry);

setTypeIterator *setTypeInitIterator(unsigned char* subject);
void setTypeReleaseIterator(setTypeIterator *si);
int setTypeNext(setTypeIterator *si ,int64_t *llele);

double zzlGetScore(unsigned char *sptr);
void zzlNext(unsigned char *zl, unsigned char **eptr, unsigned char **sptr);

unsigned char *zipmapRewind(unsigned char *zm);
unsigned char *zipmapNext(unsigned char *zm, unsigned char **key, unsigned int *klen, unsigned char **value, unsigned int *vlen);
unsigned int zipmapLen(unsigned char *zm);

uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);

unsigned char *ziplistIndex(unsigned char *zl, int index);
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p);
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p);
unsigned int ziplistGet(unsigned char *p, unsigned char **sval, unsigned int *slen, long long *lval);
unsigned int ziplistLen(unsigned char *zl);
#endif
