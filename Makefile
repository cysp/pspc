all: dumpinstrs pspc

pspc: pspc.c pspc.h
	gcc -W -Wall -g -o pspc pspc.c

dumpinstrs: dumpinstrs.c
	gcc -Wall -g -o dumpinstrs dumpinstrs.c
