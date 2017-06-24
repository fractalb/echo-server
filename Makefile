
all: echo

echo: echo-server.o
	gcc -o echo-server ./echo-server.o 

echo-server.o: echo-server.c
	gcc -c echo-server.c
