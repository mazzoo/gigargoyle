CFLAGS=-O2 -Wall -g

all:gigargoyle testpacket

gigargoyle:gigargoyle.o packets.o

clean:
	rm -f gigargoyle
	rm -f *.o
