/**
 * when a key was parsed, this function would be called once.
 * @db  : dbid 
 * @type: all are listed below example.
 * @key: key in redis db.
 * @val & @vlen: 
 * if type = REDIS_STRING, val is string, vlen is length of string.
 * if type = REDIS_SET, val is string array, vlen is num of elems in val array, 
 *     like ["1", "2", "3"], vlen=3  
 * if type = REDIS_LIST, result is same with REDIS_SET
 * if type = REDIS_ZSET, val is string array, even index is elem, odd index is score
 *     like ["elem1", 1.9, "elem2", 2.1], vlen = 4
 * if type = REDIS_HASH, val is string array, even index is field, odd index is value 
 *     like ["field1", "val1", "field2", "val2"], vlen = 4
 *
 * @expiretime: unix time in micro second. 
 *
 * Note: don't free key, val. As rdbtools would free them after call userHandler.
 */
void* userHandler (int db, int type, void *key, void *val, unsigned int vlen, long long expiretime) {

    /* I didn't use these vars in example, so mark as unused */
    V_NOT_USED(db);
    V_NOT_USED(key);
    V_NOT_USED(val);
    V_NOT_USED(vlen);
    V_NOT_USED(expiretime);
    
    unsigned int i;
    sds * val_arr;
    if(type == REDIS_STRING) {
        /* handle which type is string */
        printf("DB:%d||KEY:%s||TYPE:STRING||EXPIRE:%lld||VALUE:%s\n", db, (char *)key, expiretime, (char *) val);
    } else if (type == REDIS_SET) {
        /* handle which type is set */
        printf("DB:%d||KEY:%s||TYPE:SET||EXPIRE:%lld ||VALUE:", db, (char *)key, expiretime);
        val_arr = (sds *) val;
        for(i = 0; i < vlen; i++) {
            printf("%s,", val_arr[i]);
        }
        printf("\n");
    } else if(type == REDIS_LIST) {
        /* handle which type is list */
        printf("DB:%d||KEY:%s||TYPE:LIST||EXPIRE:%lld||VALUE:", db, (char *)key, expiretime);
        val_arr = (sds *) val;
        for(i = 0; i < vlen; i++) {
            printf("%s,", (char*) val_arr[i]);
        }
        printf("\n");
    } else if(type == REDIS_ZSET) { 
        /* handle which type is zset */
        printf("DB:%d||KEY:%s||TYPE:ZSET||EXPIRE:%lld||VALUE:", db, (char *)key, expiretime);
        val_arr = (sds *) val;
        for(i = 0; i < vlen; i += 2) {
            printf("(%s,%s) ", (char*) val_arr[i], (char*) val_arr[i+1]);
        }
        printf("\n");
    } else if(type == REDIS_HASH) {
        /* handle which type is hash */
        printf("DB:%d||KEY:%s||TYPE:HASH||EXPIRE:%lld||VALUE:", db, (char *)key, expiretime);
        val_arr = (sds *) val;
        for(i = 0; i < vlen; i += 2) {
            printf("(%s,%s) ", (char*) val_arr[i], (char*) val_arr[i+1]);
        }
        printf("\n");
    }
    printf("\n");
    return NULL;
}
