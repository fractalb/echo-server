
CC := gcc
CFLAGS := -Wall -pedantic
LDFLAGS := -lpthread

all: echo-server

echo-server: echo-server.o
	$(CC) -o echo-server ./echo-server.o $(LDFLAGS)

echo-server.o: echo-server.c
	$(CC) $(CFLAGS) -c echo-server.c

.PHONY: clean
clean:
	rm -f echo-server
	rm -f *.o
