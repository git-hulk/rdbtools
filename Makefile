objs = intset.o sds.o zmalloc.o lzf_c.o lzf_d.o adlist.o util.o ziplist.o rdb_parser.o
CC = gcc
	CFLAGS = -g -std=c99 -pedantic -Wall -W -fPIC
all: $(objs) 
	@echo "--------------------------compile start here---------------------------------"
	$(CC) $(CFLAGS) -o rdb-parser $(objs) 
	@echo "--------------------------compile  end  here---------------------------------"

crc64.o: crc64.c
lzf_c.o: lzf_c.c lzfP.h
lzf_d.o: lzf_d.c lzfP.h
rdb_parser.o: rdb_parser.c rdb_parser.h zmalloc.h sds.h adlist.h \
  ziplist.h intset.h fmacros.h lzf.h zipmap.c endian.h t_list.c t_set.c \
  t_zset.c crc64.c
sds.o: sds.c sds.h zmalloc.h
util.o: util.c
zmalloc.o: zmalloc.c config.h zmalloc.h
adlist.o: adlist.c zmalloc.h
util.o: util.c fmacros.h util.h
ziplist.o: ziplist.c zmalloc.h util.h ziplist.h endian.h
intset.o: intset.c intset.h zmalloc.h endian.h
clean:
	rm *.o rdb-parser
