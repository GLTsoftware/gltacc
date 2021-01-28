CC = gcc 
CFLAGS = -g -O -Wall -I/usr/local/include/hiredis
LIB =  -ldsm -lhiredis -lpthread -lrt -lm

OBSBIN = ./

SOURCES = ehtMonRedis.c 

OBJECTS = ehtMonRedis.o

all: ehtMonRedis

clean:
	rm *.o ./ehtMonRedis

install: all
	cp ehtMonRedis $(OBSBIN)/

ehtMonRedis: $(OBJECTS) ./Makefile
	$(CC) -o ehtMonRedis $(OBJECTS) $(LIB)

depend: ./Makefile
	$(CC) -MM $(SOURCES) > dependencies

include dependencies


