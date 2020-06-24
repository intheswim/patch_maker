CC=c++

CFLAGS = -std=c++11 -Wall -O2
CLIBS = -lm

all : main.cpp timeutil checksum  progargs patchMaker patchApplier common
		$(CC) $(CFLAGS) -o patch main.cpp $(CLIBS) checksum.o timeutil.o progArgs.o patchMaker.o patchApplier.o common.o

patchMaker : patchMaker.cpp common.h
		$(CC) $(CFLAGS) -c patchMaker.cpp $(CLIBS) 

patchApplier : patchApplier.cpp common.h
		$(CC) $(CFLAGS) -c patchApplier.cpp $(CLIBS)

checksum : checksum.cpp checksum.h
		$(CC) $(CFLAGS) -c checksum.cpp $(CLIBS)

timeutil : timeutil.cpp timeutil.h
		$(CC) $(CFLAGS) -c timeutil.cpp $(CLIBS)

progargs : progArgs.cpp progArgs.h
		$(CC) $(CFLAGS) -c progArgs.cpp $(CLIBS)

common : common.cpp common.h
		$(CC) $(CFLAGS) -c common.cpp $(CLIBS)

.PHONY: clean

clean :
		-rm timeutil.o checksum.o progArgs.o patchMaker.o patchApplier.o common.o patch