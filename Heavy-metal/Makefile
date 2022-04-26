videobox: videobox.o player.o pump.o showboard.o
	gcc $(CFLAGS) -o videobox videobox.o player.o pump.o showboard.o

player.o: player.c player.h
	gcc $(CFLAGS) -c player.c

pump.o: pump.c pump.h
	gcc $(CFLAGS) -c pump.c

showboard.o: showboard.c showboard.h
	gcc $(CFLAGS) -c showboard.c

videobox.o: videobox.c
	gcc $(CFLAGS) -c videobox.c

clean:
	rm -f *.o videobox

