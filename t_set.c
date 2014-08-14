#include "rdb_parser.h"

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
