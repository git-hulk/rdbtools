CC = gcc
CFLAGS = -g -Wall
CINCLUDES = -I../deps/lua/src
CLIBS = ../deps/lua/src/liblua.a -lm -ldl

PROG = rdbtools 

all: deps $(PROG)
.PHONY: all

OBJS = lzf_d.o rdb.o util.o ziplist.o intset.o zipmap.o endian.o crc64.o log.o script.o main.o

rdbtools: $(OBJS)
	$(CC) $(CFLAGS) $(CINCLUDES) -o $(PROG) $(OBJS) $(CLIBS)

crc64.o: crc64.c crc64.h
log.o: log.c log.h
endian.o: endian.c endian.h
intset.o: intset.c intset.h endian.h
lzf_d.o: lzf_d.c lzfP.h
main.o: main.c rdb.h script.h ../deps/lua/src/lua.h \
  ../deps/lua/src/luaconf.h ../deps/lua/src/lauxlib.h \
  ../deps/lua/src/lualib.h
rdb.o: rdb.c util.h log.h lzf.h intset.h ziplist.h script.h \
  ../deps/lua/src/lua.h ../deps/lua/src/luaconf.h \
  ../deps/lua/src/lauxlib.h ../deps/lua/src/lualib.h zipmap.h crc64.h \
  endian.h
script.o: script.c script.h log.h ../deps/lua/src/lua.h \
  ../deps/lua/src/luaconf.h ../deps/lua/src/lauxlib.h \
  ../deps/lua/src/lualib.h
util.o: util.c util.h
ziplist.o: ziplist.c ziplist.h script.h ../deps/lua/src/lua.h \
  ../deps/lua/src/luaconf.h ../deps/lua/src/lauxlib.h \
  ../deps/lua/src/lualib.h util.h endian.h
zipmap.o: zipmap.c zipmap.h script.h ../deps/lua/src/lua.h \
  ../deps/lua/src/luaconf.h ../deps/lua/src/lauxlib.h \
  ../deps/lua/src/lualib.h endian.h

%.o: %.c 
	$(CC) $(CFLAGS) $(CINCLUDES)  -c $<

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
ifeq ($(uname_S),SunOS)
	os_Platform := solaris 
else
ifeq ($(uname_S),Darwin)
	os_Platform := macosx
else
ifeq ($(uname_S),AIX)
	os_Platform := aix
else
	os_Platform := linux
endif
endif
endif

deps:
	@cd ../deps/lua/src && make $(os_Platform)

clean:
	- rm -rf *.o $(PROG)
	@cd ../deps/lua/src/ && make clean
