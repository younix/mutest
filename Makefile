CFLAGS= -std=c99 -pedantic -Wall -Wextra

.PHONY: all clean

all: send recv
clean:
	rm -f send recv
