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

int runserver(char *ipaddr, int port, FILE *local_file);
void print_client_sockaddr(struct sockaddr_in *client);

void print_client_sockaddr(struct sockaddr_in *client)
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

struct server_args {
	int sockfd;
	FILE *local_file;
};

static void do_echo(int fd, char *buf, int size, FILE *local_file)
{
	int i, j, k;

	if (fd < 0 || buf == NULL || size < 1)
		return;

	while ((i = recv(fd, buf, size, 0)) > 0) {
		if (local_file)
			fwrite(buf, 1, i, stdout);

		for (k = 0; k != i; k += j) {
			if ((j = send(fd, buf + k, i - k, 0)) < 0) {
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
	if (fd >= 0)
		close(fd);
	/*
	 * closing local file stream is not needed
	 * since it's common to all the threads
	 */
	return;
}

static void *start_server_thread(void *arg)
{
	char *buf = NULL;
	const int size = READBUF_SIZE;
	struct server_args *s = arg;
	if (s == NULL)
		goto err;

	buf = malloc(size);
	if (buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		goto err;
	}

	do_echo(s->sockfd, buf, size, s->local_file);
	free(buf);
err:
	return NULL;
}

/*
 * This serer can handle only one client at a time
 */
int runserver(char *ipaddr, int port, FILE *local_file)
{
	int sockfd = -1;
	int new_fd = -1;
	struct server_args sargs = { 0 };
	struct in_addr ip = { 0 };
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	pthread_t tinfo;
	pthread_attr_t tattr;
	socklen_t clnt_len;

	if (pthread_attr_init(&tattr) != 0 ||
	    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) != 0) {
		fprintf(stderr, "Thread attributes initialization failed");
		goto err;
	}

	sargs.local_file = local_file;

	if (ipaddr == NULL) {
		ip.s_addr = htonl(INADDR_ANY);
	} else if (inet_pton(AF_INET, ipaddr, &ip) != 1) {
		fprintf(stderr, "%s : Not a valid IPv4 address", ipaddr);
		goto err;
	}

	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Unable to create a socket. errno=%d\n", errno);
		goto err;
	}

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = ip.s_addr;

	if (bind(sockfd, (struct sockaddr *)&server, sizeof(server))) {
		fprintf(stderr, "Bind failed. errno=%d\n", errno);
		goto err;
	}

	if (listen(sockfd, LISTEN_BACKLOG)) {
		fprintf(stderr, "Listen failed. errno=%d\n", errno);
		goto err;
	}

	while (true) {
		print_server_addr_and_port(ipaddr, port);

		clnt_len = sizeof(client);
		memset(&client, 0, clnt_len);

		new_fd = accept(sockfd, (struct sockaddr *)&client, &clnt_len);
		if (new_fd < 0) {
			fprintf(stderr, "Accept failed. errno=%d\n", errno);
			goto err;
		}

		print_client_sockaddr(&client);
		sargs.sockfd = new_fd;
		memset(&tinfo, 0, sizeof(tinfo));
		pthread_create(&tinfo, &tattr, start_server_thread, &sargs);
	}

err:
	if (sockfd > -1)
		close(sockfd);

	return FAILURE;
}

int main(int argc, char *argv[])
{
	int port = 0;
	char *ip = NULL;
	bool echo_locally = false;
	FILE *local_file = NULL;

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
			fprintf(stderr, "malformed command line\n");
			return FAILURE;
		}
	}

	port = port ? port : LISTEN_PORT;
	local_file = echo_locally ? stdout : NULL;

	return runserver(ip, port, local_file);
}
