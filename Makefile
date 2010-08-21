#CC=diet gcc -Os
CFLAGS=-O2 -Wall -g

CFLAGS+=-DHAS_ARGP_ERROR  # <- enable these for linux/glibc
CFLAGS+=-DHAS_ARGP_USAGE  # <- enable these for linux/glibc
CFLAGS+=-DHAS_ARGP_PARSE  # <- enable these for linux/glibc

OBJS=gigargoyle.o command_line_arguments.o fifo.o packets.o

all:gigargoyle testpacket

gigargoyle: $(OBJS)

clean:
	rm -f gigargoyle $(OBJS)
