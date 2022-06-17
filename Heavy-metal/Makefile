.PHONY: all clean

videobox: videobox.o player.o pump.o showboard.o
	gcc $(CFLAGS) -o videobox videobox.o player.o pump.o showboard.o

player.o: player.c player.h defines.h
	gcc $(CFLAGS) -c player.c

pump.o: pump.c pump.h defines.h
	gcc $(CFLAGS) -c pump.c

showboard.o: showboard.c showboard.h defines.h
	gcc $(CFLAGS) -c showboard.c

videobox.o: videobox.c defines.h
	gcc $(CFLAGS) -c videobox.c

clean:
	rm -f *.o videobox

