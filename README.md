rdb_tools
=========

#### 1. what about this tool todo?

> parse rdb file to user format data, like aof file, json etc.

#### 2. how to use?

>1. cd rdb_tools(or rdb_tools/src)
2. make, and will be a binary file named rdb-parser in src.
3. ./rdb-parser -f dump.rdb

#### 3. how to define user's handle for differnt data type.
> open src/main.c you can see an example function called userHandler, and now just print data to screen. it's prototype is below:

```c
/*
 * user can define handler to different data type.
 
 * @param type: may be STRING, SET, LIST, ZSET, HASH 
 *    if type == STRING, val is sds.
 *    if type == SET, val is array and element type is sds. like ["1", "3", "7"]
 *    if type == LIST,val is array and element type is sds. like ["a", "b", "c"]
 *    if type == ZSET,val is array and element type is sds. like ["member1", "score1", "member2", "score2"]
 *    if type == HASH,val is array and element type is sds. like ["key1", "val1", "key2", "val2"]
 * @param key:  key is key object in redis, it's type is sds.
 * @param val:  val is denpend on type.
 * @ vlen, if type == STRING, vlen represent length of val string, or vlen is length of val array.
 * @ expiretime, expiretime in db,if not set, expiretime = -1.
 */
 
void keyValueHandler(int type, void *key, void *val, unsigned int vlen, time_t expiretime);
```

