#include "rdb_parser.h"

/*-----------------------------------------------------------------------------
 * List API
 *----------------------------------------------------------------------------*/

//todo: used
/* Initialize an iterator at the specified index. */
listTypeIterator *listTypeInitIterator(unsigned char *subject, int index, unsigned char direction) {
    listTypeIterator *li = zmalloc(sizeof(listTypeIterator));
    li->subject = subject;
    li->direction = direction;
    li->zi = ziplistIndex(subject,index);
    return li;
}

//todo: used
/* Clean up the iterator. */
void listTypeReleaseIterator(listTypeIterator *li) {
    zfree(li);
}

//todo:used
/* Stores pointer to current the entry in the provided entry structure
 * and advances the position of the iterator. Returns 1 when the current
 * entry is in fact an entry, 0 otherwise. */
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
