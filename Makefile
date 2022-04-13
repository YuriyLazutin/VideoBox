videobox: videobox.o player.o
	gcc $(CFLAGS) -o videobox videobox.o player.o

player.o: player.c player.h
	gcc $(CFLAGS) -c player.c

videobox.o: videobox.c
	gcc $(CFLAGS) -c videobox.c

clean:
	rm -f *.o videobox

