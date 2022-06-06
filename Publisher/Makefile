.PHONY: all clean

all: publisher

publisher: publisher.o
	gcc $(CFLAGS) -o publisher publisher.o

publisher.o: publisher.c defines.h
	gcc $(CFLAGS) -c publisher.c

clean:
	rm -f *.o publisher

