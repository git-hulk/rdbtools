rdbtools
=========

A tool use c to analyze redis rdb file, and use lua to handle.

> Notice: This tools was tested on 2.2 , 2.4 , 2.6, 2.8

> bugs in other versions? open issues, I will fix it, thanks~



#### 1. What about this tool to do? 

> use lua to parse rdb file to user's format data, like aof file, json etc.


#### 2. How to use?
```shell
$ cd rdbtools/src
$ make
$ ./rdbtools -f ../tests/dump2.4.rdb -s ../scripts/example.lua
```

#### 3. Options

```shell
USAGE: ./rdbtools [-f file] -V -h
    -V --version 
    -h --help show usage 
    -f --file specify which rdb file would be parsed.
    -s --file specify which lua script, default is ../scripts/example.lua
```

If you want to handle key-value in rdb file, you can use `-s your_script.lua`, and lua function `handle` will be callbacked.

Example can be found in `scripts/example.lua`, and it just print the key-value.


##### Json format example

`cat scripts/json_exapmle.lua`

```lua
local cjson = require "cjson"

function handle(item)
     print(cjson.encode(item))
end     
```

And result is below:
![image](https://raw.githubusercontent.com/git-hulk/rdbtools/master/snapshot/rdbtools-to-json.png)

#### 4. Params in handle function

```lua
function handle(item)
 --item.type, value may be string, set, zset, list, or hash.
 --item.expire_time, key expire time .
 --item.value, may be string or list or hash, it depends on item.type
end
```


#### 5. Environment

If you what to know rdb version and which db is selcted.

```lua
print(env.version)
print(env.db_num)
```

#### 6. Contact me?
> ```Sina Weibo```: [@改名hulk](http://www.weibo.com/tianyi4)

>```Gmail```: [hulk.website@gmail.com](mailto:hulk.website@gmail.com)

>```Blog```: [www.hulkdev.com](http://www.hulkdev.com)

any bugs? send mail, and I will appreciate your help.

