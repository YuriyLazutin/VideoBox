videobox: videobox.o
	gcc $(CFLAGS) -o videobox videobox.o

videobox.o: videobox.c
	gcc $(CFLAGS) -c videobox.c

clean:
	rm -f *.o videobox

