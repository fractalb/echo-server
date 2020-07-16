#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define READBUF_SIZE 32
#define LISTEN_BACKLOG 2
#define LISTEN_PORT 8001
#define FAILURE (-1)

int runserver(char *ipaddr, int port, FILE *file_out);
void print_sockaddr(struct sockaddr_in *client);

/**
 * Prints the client's socket address ip+port) when a
 * client connection is successfully established
 */
void print_sockaddr(struct sockaddr_in *client)
{
	char buf[INET_ADDRSTRLEN];
	const void *ret;

	ret = inet_ntop(AF_INET, &client->sin_addr, buf, sizeof(buf));
	if (ret == NULL) {
		fprintf(stderr,
			"Connection established, but unable to get the client details.\n");
		return;
	}

	printf("Accepted a connection from %s:%d\n", buf,
	       ntohs(client->sin_port));
	return;
}

static void print_server_addr_and_port(const char *ipaddr, int port)
{
	if (ipaddr)
		printf("Listening on %s:%d\n", ipaddr, port);
	else
		printf("Listening on port %d\n", port);

	return;
}

struct client_args {
	int sockfd;
	FILE *file_out;
};

/**
 * Read data from the socket fd and echo it back to the same `sockfd`
 * `file_out`, if not NULL, is used for logging the data
 * `buf` temporarily holds read data before echoing back
 */
static void do_echo(int sockfd, char *buf, int size, FILE *file_out)
{
	int i, j, k;

	if (sockfd < 0 || buf == NULL || size < 1)
		return;

	while ((i = recv(sockfd, buf, size, 0)) > 0) {
		if (file_out)
			fwrite(buf, 1, i, file_out);

		for (k = 0; k != i; k += j) {
			if ((j = send(sockfd, buf + k, i - k, 0)) < 0) {
				fprintf(stderr, "Send failure. errno=%d\n",
					errno);
				goto err;
			}
		}
	}

	if (i == 0)
		printf("\nConnection closed\n");
	else
		fprintf(stderr, "Read error: %d\n", errno);

err:
	return;
}

static void *start_server_thread(void *client)
{
	const struct client_args *cl = client;
	const int size = READBUF_SIZE;
	char *buf =  malloc(size);

	if (cl == NULL)
		goto err;

	if (buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		goto err;
	}

	/* Start echoing */
	do_echo(cl->sockfd, buf, size, cl->file_out);

	close(cl->sockfd);
	free(buf);
err:
	return NULL;
}

/**
 * Create a server listening on `ipaddr`:`port`
 * write data received from client to `file_out`
 */
int runserver(char *ipaddr, int port, FILE *file_out)
{
	struct client_args clnt_args  = { .file_out = file_out };
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	struct in_addr ip         = { 0 };

	int sockfd = -1;
	int new_fd = -1;

	pthread_t tinfo;
	pthread_attr_t tattr;
	socklen_t client_len;

	if (pthread_attr_init(&tattr) != 0 ||
	    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) != 0) {
		fprintf(stderr, "Thread attributes initialization failed");
		goto err;
	}

	/* local IP address to lisdten on */
	if (ipaddr == NULL) {
		ip.s_addr = htonl(INADDR_ANY);
	} else if (inet_pton(AF_INET, ipaddr, &ip) != 1) {
		fprintf(stderr, "%s : Not a valid IPv4 address", ipaddr);
		goto err;
	}

	/* Create server socket */
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Unable to create a socket. errno=%d\n", errno);
		goto err;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port); /* local port to listen on */
	server.sin_addr.s_addr = ip.s_addr;

	if (bind(sockfd, (struct sockaddr *)&server, sizeof(server))) {
		fprintf(stderr, "Socket bind failed. errno=%d\n", errno);
		goto err;
	}

	if (listen(sockfd, LISTEN_BACKLOG)) {
		fprintf(stderr, "Listen failed. errno=%d\n", errno);
		goto err;
	}

	while (true) {
		print_server_addr_and_port(ipaddr, port);

		/* client socket struct to store the client details */
		client_len = sizeof(client);
		memset(&client, 0, client_len);

		new_fd = accept(sockfd, (struct sockaddr *)&client, &client_len);
		if (new_fd < 0) {
			fprintf(stderr, "Accept failed. errno=%d\n", errno);
			goto err;
		}

		print_sockaddr(&client); /* print the client address on stdout */
		clnt_args.sockfd = new_fd;
		memset(&tinfo, 0, sizeof(tinfo));
		pthread_create(&tinfo, &tattr, start_server_thread, &clnt_args);
	}

err:
	/* Close the listening socket */
	if (sockfd > -1)
		close(sockfd);

	return FAILURE;
}

int main(int argc, char *argv[])
{
	int port = 0;
	char *ip = NULL;
	bool echo_locally = false;
	FILE *file_out = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-e") == 0) {
			/* Echo locally */
			echo_locally = true;
		} else if (strcmp(argv[i], "-ip") == 0) {
			i++;
			/* port number. listen on this port */
			ip = argv[i];
		} else if (port == 0) {
			port = atoi(argv[i]);
		} else {
			fprintf(stderr, "Error parsing command line\n");
			return FAILURE;
		}
	}

	port = port ? port : LISTEN_PORT;
	file_out = echo_locally ? stdout : NULL;

	return runserver(ip, port, file_out);
}
