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
#endif
