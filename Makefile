CFLAGS=-O2 -Wall

all:gigargoyle

gigargoyle:gigargoyle.o packets.o

clean:
	rm -f gigargoyle
	rm -f *.o
