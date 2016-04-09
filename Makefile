CC=gcc
CFLAGS="-Wall"

debug:clean
	$(CC) $(CFLAGS) -g -o simplevncserver main.c
stable:clean
	$(CC) $(CFLAGS) -o simplevncserver main.c
clean:
	rm -vfr *~ simplevncserver
