# makefile for rrsh

CFLAGS = -Wall -O3

rrsh-server: rrshServer.c rrshServer.h
	gcc ${CFLAGS} rrshServer.c -o rrshServer

clean:
	rm -f rrshServer
