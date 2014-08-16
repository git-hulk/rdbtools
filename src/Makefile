objs = intset.o sds.o  endian.o  zmalloc.o  zipmap.o lzf_c.o lzf_d.o util.o ziplist.o rdb_parser.o main.o
CC = gcc
	CFLAGS = -g -std=c99 -pedantic -Wall -W -fPIC
all: $(objs) 
	@echo "--------------------------compile start here---------------------------------"
	$(CC) $(CFLAGS) -o rdb-parser $(objs) 
	@echo "--------------------------compile  end  here---------------------------------"

intset.o: intset.c intset.h zmalloc.h endian.h
lzf_c.o: lzf_c.c lzfP.h
lzf_d.o: lzf_d.c lzfP.h
main.o: main.c main.h zmalloc.h sds.h fmacros.h rdb_parser.h
rdb_parser.o: rdb_parser.c rdb_parser.h main.h zmalloc.h sds.h fmacros.h \
 intset.h ziplist.h zipmap.h t_list.c t_set.c t_zset.c lzf.h crc64.c
sds.o: sds.c sds.h zmalloc.h
t_list.o: t_list.c main.h zmalloc.h sds.h fmacros.h
t_set.o: t_set.c main.h zmalloc.h sds.h fmacros.h
t_zset.o: t_zset.c main.h zmalloc.h sds.h fmacros.h
util.o: util.c fmacros.h util.h
ziplist.o: ziplist.c zmalloc.h util.h ziplist.h endian.h
zipmap.o: zipmap.c zmalloc.h endian.h
zmalloc.o: zmalloc.c config.h zmalloc.h
clean:
	rm *.o rdb-parser
