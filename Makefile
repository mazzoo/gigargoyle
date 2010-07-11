CFLAGS=-O2 -Wall -g

all:gigargoyle testpacket

gigargoyle:gigargoyle.o packets.o fifo.o

clean:
	rm -f gigargoyle testpacket
	rm -f *.o
