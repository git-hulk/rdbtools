CC = gcc
CFLAGS = -g -Wall

PROG = rdbtools 

all: $(PROG)
.PHONY: all

OBJS = lzf_d.o rdb.o util.o ziplist.o intset.o zipmap.o endian.o

rdbtools: $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)
	
endian.o: endian.c endian.h
intset.o: intset.c intset.h endian.h
lzf_d.o: lzf_d.c lzfP.h
rdb.o: rdb.c util.h lzf.h intset.h ziplist.h zipmap.h
util.o: util.c util.h
ziplist.o: ziplist.c ziplist.h endian.h
zipmap.o: zipmap.c zipmap.h endian.h

clean:
	- rm -rf *.o $(PROG)
