#
# Students' Makefile for the Malloc Lab
#
CC = gcc
CFLAGS = -Wall -g -pg

OBJS = mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o
OBJ2 = mm.o memlib.o  fcyc.o clock.o ftimer.o



test: test.o $(OBJ2)
	$(CC) $(CFLAGS) -o test test.o $(OBJ2)

mdriver: mdriver.o $(OBJS)
	$(CC) $(CFLAGS) -o mdriver mdriver.o $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
test.o: test.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h


memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h
test.o: test.c mm.h

clean:
	rm -f *~ *.o mdriver
