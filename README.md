rdb_tools
=========

> Notice: This tools was tested on 2.2 , 2.4 , 2.6, 2.8

> bugs in other versions? open issues, I will fix it, thanks~


#### 1. what about this tool to do? 

> parse rdb file to user's format data, like aof file, json etc.

#### 2. why write this?
> rdb file parser can be used in many places, and there are too many data types in redis, also too complicated for many developers, so I write this. By parsing rdb file to simple format data seems like json, but difference. like below:
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
$ ./rdb-tools -f ../tests/dump2.4.rdb 
```

#### 4. how to define user's handle for differnt data type.

```shell
$ cd src/
$ vi callbacks.c
$ # edit method userHandler as you like, good luck!
```

> Note: 

> Remeber don't free any parameters (like key, vals) in userHandler, as we do that already.

> Prototype is below:

```c
/*
 * user can define handler to different data type.
 * @db : db id; 
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
 * @ return any data as you like.
 */
 
void* userHandler(int db, int type, void *key, void *val, unsigned int vlen, long long expiretime);
```

#### 5. test snapshot
![image](https://github.com/git-hulk/rdbtools/blob/master/snapshot/rdb-tools.png)

#### 6. contact me?
> ```Sina Weibo```: [@改名hulk](http://www.weibo.com/tianyi4)

>```Gmail```: [hulk.website@gmail.com](mailto:hulk.website@gmail.com)

>```Blog```: [www.hulkdev.com](http://hulkdev.sinaapp.com)

any bugs? send mail, and I will appreciate your help.
