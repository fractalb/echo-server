
all: echo-server

echo-server: echo-server.o
	gcc -o echo-server ./echo-server.o 

echo-server.o: echo-server.c
	gcc -c echo-server.c

.PHONY: clean
clean:
	rm echo-server
	rm -f *.o
