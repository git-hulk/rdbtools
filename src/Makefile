objs = sds.o  endian.o  lzf_c.o lzf_d.o util.o rdb_parser.o main.o
CC = gcc
CFLAGS = -g -std=c99 -pedantic -Wall -W -fPIC 
all: $(objs) 
	@echo "--------------------------compile start here---------------------------------"
	$(CC) $(CFLAGS) -o rdb-tools $(objs) 
	@echo "--------------------------compile  end  here---------------------------------"

crc64.o: crc64.c
endian.o: endian.c
lzf_c.o: lzf_c.c lzfP.h
lzf_d.o: lzf_d.c lzfP.h
main.o: main.c main.h sds.h    \
  lzf.h rdb_parser.h callbacks.c
rdb_parser.o: rdb_parser.c rdb_parser.h main.h sds.h  \
    lzf.h crc64.c util.h
sds.o: sds.c sds.h 
util.o: util.c  main.h sds.h   \
  lzf.h
clean:
	-rm *.o rdb-tools
