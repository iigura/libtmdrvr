# Tiny Mouse Driver without X
# written by Koji Iigura 2019.
# please see the README.md.

CFLAGS=-std=gnu99 -Wextra -Wall

test: test.c libtmdrvr.a
	gcc $(CFLAGS) -pthread test.c -L. -ltmdrvr -o test

libtmdrvr.a: tmDriver.o
	ar r libtmdrvr.a tmDriver.o

.SUFFIX: .c .o
.c.o:
	gcc $(CFLAGS) -fPIC -Wall -Wextra $(INCLUDE_PATHS) -c $<
	
.PHONY: clean
clean:
	rm -rf tmDriver.o

