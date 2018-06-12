
CC := gcc
CFLAGS := -Wall -pedantic
LDFLAGS :=

all: echo-server

echo-server: echo-server.o
	$(CC) $(LDFLAGS) -o echo-server ./echo-server.o

echo-server.o: echo-server.c
	$(CC) $(CFLAGS) -c echo-server.c

.PHONY: clean
clean:
	rm -f echo-server
	rm -f *.o
