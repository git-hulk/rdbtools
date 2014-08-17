rdb_tools
=========

> Notice: 

>1. many file was copy from redis, like sds, malloc without change, I think it's not necessary to rewrite again, but some like t_set, t_list, t_zset, we just take what we need, it's need a little function, like iterator.

>2. This can't only test in redis branch 2.2 & 2.4 in github, it would be error when use in redis 2.4 later.


#### 1. what about this tool todo? 

> parse rdb file to user format data, like aof file, json etc.

#### 2. why write this?
> rdb file parser can be use in many place, and there are too many data types in redis, also too complicated to many delelopers. so i write this tool to resolve it, by just parse rdb file to simple format data seems like json but difference. like below:
```
 if type is STRING, value like "value1"
 if type is SET, value is array and element type is string. like ["1", "3", "7"]
 if type is LIST,value is array and element type is string. like ["a", "b", "c"]
 if type is ZSET,value is array and element type is string. like ["member1", "score1", "member2", "score2"]
 if type is HASH,value is array and element type is string. like ["key1", "val1", "key2", "val2"]
```

#### 3. how to use?
```shell
$ cd rdb_tools # (or rdb_tools/src)
$ make # and will be a binary file named rdb-parser in src.
$ ./rdb-parser -f dump.rdb # start parse
```

#### 4. how to define user's handle for differnt data type.
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

#### 5. test snapshot
![image](https://github.com/git-hulk/rdbtools/blob/master/snapshot/rdb-tools.png)

#### 6. contact me?
> hulk.website@gmail.com

