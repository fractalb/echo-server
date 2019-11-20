#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define READBUF_SIZE 20
#define LISTEN_BACKLOG 2
#define LISTEN_PORT 8001
#define FAILURE (-1)

int runserver(char *ipaddr, int port, bool echo_locally);
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

static void do_echo(int fd, char *buf, int size, bool echo_locally)
{
	int i, j, k;

	if (fd < 0 || buf == NULL || size < 2)
		return;

	while ((i = recv(fd, buf, READBUF_SIZE, 0)) > 0) {
		if (echo_locally) {
			buf[i] = '\0';
			fputs(buf, stdout);
		}

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
	close(fd);
	return;
}

int runserver(char *ipaddr, int port, bool echo_locally)
{
	int sockfd = -1;
	int new_fd = -1;
	char *buf = NULL;
	struct in_addr ip = { 0 };
	struct sockaddr_in server = { 0 };
	struct sockaddr_in client = { 0 };
	socklen_t clnt_len;

	port = port ? port : LISTEN_PORT;

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

	/*
	 * READBUF_SIZE + 1 for NUL('\0') character
	 * This same buffer is reused to read data from
	 * all the clients. We handle only one client at a time
	 */
	buf = malloc(READBUF_SIZE + 1);
	if (buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
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
		do_echo(new_fd, buf, READBUF_SIZE, echo_locally);
	}

err:
	free(buf);

	if (sockfd > -1)
		close(sockfd);

	return FAILURE;
}

int main(int argc, char *argv[])
{
	int port = 0;
	char *ip = NULL;
	bool echo_locally = false;
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

	return runserver(ip, port, echo_locally);
}
