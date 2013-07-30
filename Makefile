CC = gcc

CFLAGS = -g -Wall -Os -s

LIBS = -lX11 -L/usr/X11R6/lib

TARGETS = dclock

all:	$(TARGETS)

dclock: dclock.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

.c.o: $<
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)
