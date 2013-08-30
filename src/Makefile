CC=gcc
CFLAGS=-Wall -g
AR=ar

OBJS= ehash.o cache.o freemap.o bucket.o \
	merge.o tmpindex.o	\
	vbyte.o vector.o index.o  \
	hash.o filemap.o
SRCS=$(OBJS:.o=.c)

%.o:%.c
	$(CC) -c $(CFLAGS) $< -o $@

all:.depend libindex.a

.depend:
	$(CC) -MM $(CFLAGS) $(SRCS) > .depend
libindex.a:$(OBJS)
	$(AR) rs $@ $(OBJS)

-include .depend

clean:
	rm -f *.o .depend libindex.a

vector:vector.c vbyte.c vbyte.h
	gcc -g -DVECTOR_TEST vector.c vbyte.c -o vector
bucket:CFLAGS+=-DBUCKET_TEST
bucket:bucket.o hash.o
	$(CC) $^ -o $@
freemap:freemap.c
	gcc -g -DFREEMAP_TEST freemap.c -o freemap
tmpindex:tmpindex.c vector.c vbyte.c hash.c
	gcc -g -DTMPINDEX_TEST tmpindex.c vector.c vbyte.c hash.c -o tmpindex
merge:merge.c tmpindex.c vector.c vbyte.c hash.c bucket.c
	gcc -g -DMERGE_TEST merge.c vector.c vbyte.c tmpindex.c hash.c bucket.c freemap.c cache.c ehash.c -o merge
ehash:ehash.c cache.c freemap.c bucket.c ehash.h cache.h freemap.h bucket.h ehash.h
	gcc -g -DEHASH_TEST ehash.c cache.c freemap.c bucket.c hash.c -o ehash
index: $(OBJS)
	$(CC) $(OBJS) -lm -o index
#index:CFLAGS+=-DINDEX_TEST

generator:generator.o hash.o
	$(CC) $^ -o $@
