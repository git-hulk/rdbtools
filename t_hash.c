#include "rdb_parser.h"

#include <math.h>

/*-----------------------------------------------------------------------------
 * Hash type API
 *----------------------------------------------------------------------------*/

/* Check the length of a number of objects to see if we need to convert a
 * zipmap to a real hash. Note that we only check string encoded objects
 * as their string length can be queried in constant time. */
void hashTypeTryConversion(robj *subject, robj **argv, int start, int end) {
    int i;
    if (subject->encoding != REDIS_ENCODING_ZIPMAP) return;

    for (i = start; i <= end; i++) {
        if (argv[i]->encoding == REDIS_ENCODING_RAW &&
            sdslen(argv[i]->ptr) > server->hash_max_zipmap_value)
        {
            convertToRealHash(subject);
            return;
        }
    }
}

/* Encode given objects in-place when the hash uses a dict. */
void hashTypeTryObjectEncoding(robj *subject, robj **o1, robj **o2) {
    if (subject->encoding == REDIS_ENCODING_HT) {
        if (o1) *o1 = tryObjectEncoding(*o1);
        if (o2) *o2 = tryObjectEncoding(*o2);
    }
}

/* Get the value from a hash identified by key.
 *
 * If the string is found either REDIS_ENCODING_HT or REDIS_ENCODING_ZIPMAP
 * is returned, and either **objval or **v and *vlen are set accordingly,
 * so that objects in hash tables are returend as objects and pointers
 * inside a zipmap are returned as such.
 *
 * If the object was not found -1 is returned.
 *
 * This function is copy on write friendly as there is no incr/decr
 * of refcount needed if objects are accessed just for reading operations. */
int hashTypeGet(robj *o, robj *key, robj **objval, unsigned char **v,
                unsigned int *vlen)
{
    if (o->encoding == REDIS_ENCODING_ZIPMAP) {
        int found;

        key = getDecodedObject(key);
        found = zipmapGet(o->ptr,key->ptr,sdslen(key->ptr),v,vlen);
        decrRefCount(key);
        if (!found) return -1;
    } else {
        dictEntry *de = dictFind(o->ptr,key);
        if (de == NULL) return -1;
        *objval = dictGetEntryVal(de);
    }
    return o->encoding;
}

/* Higher level function of hashTypeGet() that always returns a Redis
 * object (either new or with refcount incremented), so that the caller
 * can retain a reference or call decrRefCount after the usage.
 *
 * The lower level function can prevent copy on write so it is
 * the preferred way of doing read operations. */
robj *hashTypeGetObject(robj *o, robj *key) {
    robj *objval;
    unsigned char *v;
    unsigned int vlen;

    int encoding = hashTypeGet(o,key,&objval,&v,&vlen);
    switch(encoding) {
        case REDIS_ENCODING_HT:
            incrRefCount(objval);
            return objval;
        case REDIS_ENCODING_ZIPMAP:
            objval = createStringObject((char*)v,vlen);
            return objval;
        default: return NULL;
    }
}

/* Test if the key exists in the given hash. Returns 1 if the key
 * exists and 0 when it doesn't. */
int hashTypeExists(robj *o, robj *key) {
    if (o->encoding == REDIS_ENCODING_ZIPMAP) {
        key = getDecodedObject(key);
        if (zipmapExists(o->ptr,key->ptr,sdslen(key->ptr))) {
            decrRefCount(key);
            return 1;
        }
        decrRefCount(key);
    } else {
        if (dictFind(o->ptr,key) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* Add an element, discard the old if the key already exists.
 * Return 0 on insert and 1 on update. */
int hashTypeSet(robj *o, robj *key, robj *value) {
    int update = 0;
    if (o->encoding == REDIS_ENCODING_ZIPMAP) {
        key = getDecodedObject(key);
        value = getDecodedObject(value);
        o->ptr = zipmapSet(o->ptr,
            key->ptr,sdslen(key->ptr),
            value->ptr,sdslen(value->ptr), &update);
        decrRefCount(key);
        decrRefCount(value);

        /* Check if the zipmap needs to be upgraded to a real hash table */
        if (zipmapLen(o->ptr) > server->hash_max_zipmap_entries)
            convertToRealHash(o);
    } else {
        if (dictReplace(o->ptr,key,value)) {
            /* Insert */
            incrRefCount(key);
        } else {
            /* Update */
            update = 1;
        }
        incrRefCount(value);
    }
    return update;
}

/* Delete an element from a hash.
 * Return 1 on deleted and 0 on not found. */
int hashTypeDelete(robj *o, robj *key) {
    int deleted = 0;
    if (o->encoding == REDIS_ENCODING_ZIPMAP) {
        key = getDecodedObject(key);
        o->ptr = zipmapDel(o->ptr,key->ptr,sdslen(key->ptr), &deleted);
        decrRefCount(key);
    } else {
        deleted = dictDelete((dict*)o->ptr,key) == DICT_OK;
        /* Always check if the dictionary needs a resize after a delete. */
        if (deleted && htNeedsResize(o->ptr)) dictResize(o->ptr);
    }
    return deleted;
}

/* Return the number of elements in a hash. */
unsigned long hashTypeLength(robj *o) {
    return (o->encoding == REDIS_ENCODING_ZIPMAP) ?
        zipmapLen((unsigned char*)o->ptr) : dictSize((dict*)o->ptr);
}

hashTypeIterator *hashTypeInitIterator(robj *subject) {
    hashTypeIterator *hi = zmalloc(sizeof(hashTypeIterator));
    hi->encoding = subject->encoding;
    if (hi->encoding == REDIS_ENCODING_ZIPMAP) {
        hi->zi = zipmapRewind(subject->ptr);
    } else if (hi->encoding == REDIS_ENCODING_HT) {
        hi->di = dictGetIterator(subject->ptr);
    } else {
        redisAssert(NULL);
    }
    return hi;
}

void hashTypeReleaseIterator(hashTypeIterator *hi) {
    if (hi->encoding == REDIS_ENCODING_HT) {
        dictReleaseIterator(hi->di);
    }
    zfree(hi);
}

/* Move to the next entry in the hash. Return REDIS_OK when the next entry
 * could be found and REDIS_ERR when the iterator reaches the end. */
int hashTypeNext(hashTypeIterator *hi) {
    if (hi->encoding == REDIS_ENCODING_ZIPMAP) {
        if ((hi->zi = zipmapNext(hi->zi, &hi->zk, &hi->zklen,
            &hi->zv, &hi->zvlen)) == NULL) return REDIS_ERR;
    } else {
        if ((hi->de = dictNext(hi->di)) == NULL) return REDIS_ERR;
    }
    return REDIS_OK;
}

/* Get key or value object at current iteration position.
 * The returned item differs with the hash object encoding:
 * - When encoding is REDIS_ENCODING_HT, the objval pointer is populated
 *   with the original object.
 * - When encoding is REDIS_ENCODING_ZIPMAP, a pointer to the string and
 *   its length is retunred populating the v and vlen pointers.
 * This function is copy on write friendly as accessing objects in read only
 * does not require writing to any memory page.
 *
 * The function returns the encoding of the object, so that the caller
 * can underestand if the key or value was returned as object or C string. */
int hashTypeCurrent(hashTypeIterator *hi, int what, robj **objval, unsigned char **v, unsigned int *vlen) {
    if (hi->encoding == REDIS_ENCODING_ZIPMAP) {
        if (what & REDIS_HASH_KEY) {
            *v = hi->zk;
            *vlen = hi->zklen;
        } else {
            *v = hi->zv;
            *vlen = hi->zvlen;
        }
    } else {
        if (what & REDIS_HASH_KEY)
            *objval = dictGetEntryKey(hi->de);
        else
            *objval = dictGetEntryVal(hi->de);
    }
    return hi->encoding;
}

/* A non copy-on-write friendly but higher level version of hashTypeCurrent()
 * that always returns an object with refcount incremented by one (or a new
 * object), so it's up to the caller to decrRefCount() the object if no
 * reference is retained. */
robj *hashTypeCurrentObject(hashTypeIterator *hi, int what) {
    robj *obj;
    unsigned char *v = NULL;
    unsigned int vlen = 0;
    int encoding = hashTypeCurrent(hi,what,&obj,&v,&vlen);

    if (encoding == REDIS_ENCODING_HT) {
        incrRefCount(obj);
        return obj;
    } else {
        return createStringObject((char*)v,vlen);
    }
}

robj *hashTypeLookupWriteOrCreate(redisClient *c, robj *key) {
    robj *o = lookupKeyWrite(c->db,key);
    if (o == NULL) {
        o = createHashObject();
        dbAdd(c->db,key,o);
    } else {
        if (o->type != REDIS_HASH) {
            addReply(c,shared.wrongtypeerr);
            return NULL;
        }
    }
    return o;
}

void convertToRealHash(robj *o) {
    unsigned char *key, *val, *p, *zm = o->ptr;
    unsigned int klen, vlen;
    dict *dict = dictCreate(&hashDictType,NULL);

    redisAssert(o->type == REDIS_HASH && o->encoding != REDIS_ENCODING_HT);
    p = zipmapRewind(zm);
    while((p = zipmapNext(p,&key,&klen,&val,&vlen)) != NULL) {
        robj *keyobj, *valobj;

        keyobj = createStringObject((char*)key,klen);
        valobj = createStringObject((char*)val,vlen);
        keyobj = tryObjectEncoding(keyobj);
        valobj = tryObjectEncoding(valobj);
        dictAdd(dict,keyobj,valobj);
    }
    o->encoding = REDIS_ENCODING_HT;
    o->ptr = dict;
    zfree(zm);
}
