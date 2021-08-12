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
#define DEFAULT_LISTEN_PORT 8001
#define FAILURE (-1)

struct client_conn {
	int sockfd;
	struct sockaddr_in sockaddr;
	FILE *file_out;
	char ipaddr_string[INET_ADDRSTRLEN];
};

static const char *sockaddr_to_string(struct sockaddr *sockaddr, char *buf,
				      int size)
{
	struct in_addr *ipv4;
	struct in6_addr  *ipv6;
	const char *ret = NULL;

	switch(sockaddr->sa_family) {
	case AF_INET:
		ipv4 = &((struct sockaddr_in *)sockaddr)->sin_addr;
		ret = inet_ntop(AF_INET, ipv4, buf, size);
		break;
	case AF_INET6:
		ipv6 = &((struct sockaddr_in6 *)sockaddr)->sin6_addr;
		ret = inet_ntop(AF_INET6, ipv6, buf, size);
		break;
	default:
		ret = NULL;
	}

	return ret;
}

/**
 * Read data from the socket fd and echo it back to the same `sockfd`
 * `file_out`, if not NULL, is used for logging the data
 * `buf` temporarily holds read data before echoing back
 */
static void do_echo(const struct client_conn *conn, char *buf, int size)
{
	const char *ipaddr_string = conn->ipaddr_string;
	FILE *fout = conn->file_out;
	int sockfd = conn->sockfd;
	int read_bytes;
	int sent_bytes;
	int i;

	if (sockfd < 0 || buf == NULL || size < 1)
		return;

	while ((read_bytes = recv(sockfd, buf, size, 0)) > 0) {
		if (fout)
			fwrite(buf, 1, read_bytes, fout);

		/* repeatedly try until we send out all the
		 * `read_bytes` */
		for (sent_bytes = 0; sent_bytes != read_bytes;
		     sent_bytes += i) {
			if ((i = send(sockfd, buf + sent_bytes,
				      read_bytes - sent_bytes, 0)) < 0) {
				goto err_write;
			}
		}
	}

	if (read_bytes != 0)
		fprintf(stderr, "Read error:%d client %s:%d\n", errno,
			ipaddr_string, conn->sockaddr.sin_port);
	return;

err_write:
	fprintf(stderr, "Write error errno:%d client %s:%d\n", errno,
		ipaddr_string, conn->sockaddr.sin_port);
	return;
}

static void *start_server_thread(void *client)
{
	const struct client_conn *conn = client;
	const int size = READBUF_SIZE;
	char *buf = malloc(size);

	if (client == NULL)
		goto err;

	if (buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		goto err;
	}

	printf("Accepted a connection from %s:%d\n", conn->ipaddr_string,
	       conn->sockaddr.sin_port);

	/* Start echoing */
	do_echo(conn, buf, size);

	printf("Connection closed %s:%d\n", conn->ipaddr_string,
	       conn->sockaddr.sin_port);
	close(conn->sockfd);
err:
	free(client);
	free(buf);
	return NULL;
}

static struct client_conn *
init_client_conn(int sockfd, struct sockaddr *sockaddr, FILE *file_out)
{
	struct client_conn *client = malloc(sizeof(*client));
	if (client == NULL)
		return NULL;

	client->sockfd = sockfd;
	client->file_out = file_out;
	memcpy(&client->sockaddr, sockaddr, sizeof(*sockaddr));
	sockaddr_to_string(sockaddr, client->ipaddr_string, INET_ADDRSTRLEN);
	return client;
}

static int init_server(char *ipaddr, int port, struct sockaddr_in *server)
{
	struct in_addr local_ip;
	int server_fd = -1;

	memset(server, 0, sizeof(*server));
	memset(&local_ip, 0, sizeof(local_ip));

	/* local IP address to lisdten on */
	if (ipaddr == NULL)
		ipaddr = "0.0.0.0"; /* Any address */

	if (inet_pton(AF_INET, ipaddr, &local_ip) != 1) {
		fprintf(stderr, "%s : Not a valid IPv4 address", ipaddr);
		goto err;
	}

	/* Create server socket */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Unable to create a socket. errno=%d\n", errno);
		goto err;
	}

	server->sin_family = AF_INET;
	server->sin_port = htons(port); /* local port to listen on */
	server->sin_addr.s_addr = local_ip.s_addr;

	if (bind(server_fd, (struct sockaddr *)server, sizeof(*server))) {
		fprintf(stderr, "Socket bind failed. errno=%d\n", errno);
		close(server_fd);
		goto err;
	}

	if (listen(server_fd, LISTEN_BACKLOG)) {
		fprintf(stderr, "Listen failed. errno=%d\n", errno);
		close(server_fd);
		goto err;
	}

	printf("Listening on %s:%d\n", ipaddr, port);

	return server_fd;

err:
	return -1;
}

struct client_conn *accept_client_conn(int server_fd, FILE *file_out)
{
	struct client_conn *client = NULL;
	struct sockaddr client_sock;
	socklen_t sock_len;
	int client_fd = -1;

	sock_len = sizeof(client_sock);
	memset(&client_sock, 0, sock_len);
	client_fd = accept(server_fd, &client_sock, &sock_len);

	if (client_fd < 0) {
		fprintf(stderr, "Accept failed. errno:%d\n", errno);
		goto err;
	}

	client = init_client_conn(client_fd, &client_sock, file_out);

	if (client == NULL) {
		fprintf(stderr, "Internal error:%d. closing fd=%d\n", errno,
			client_fd);
		close(client_fd);
	}

err:
	return client;
}

/**
 * Create a server listening on `ipaddr`:`port`
 * write data received from client to `file_out`
 */
int runserver(char *ipaddr, int port, FILE *file_out)
{
	struct sockaddr_in server_sock;
	struct client_conn *client;
	pthread_attr_t tattr;
	pthread_t tinfo;
	int server_fd = -1;

	server_fd = init_server(ipaddr, port, &server_sock);

	if (server_fd < 0)
		goto err;

	if (pthread_attr_init(&tattr) != 0 ||
	    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) != 0)
		goto err;

	while (true) {
		client = accept_client_conn(server_fd, file_out);
		if (client == NULL)
			goto err;
		memset(&tinfo, 0, sizeof(tinfo));
		pthread_create(&tinfo, &tattr, start_server_thread, client);
	}

err:
	/* Close the listening socket */
	if (server_fd > -1)
		close(server_fd);

	return FAILURE;
}

int main(int argc, char *argv[])
{
	int port = DEFAULT_LISTEN_PORT;
	char *local_ip = NULL;
	FILE *file_out = NULL;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-e") == 0) {
			/* Echo locally */
			file_out = stdout;
		} else if (strcmp(argv[i], "-ip") == 0) {
			/* bind to this IP address */
			local_ip = argv[++i];
		} else if (i == argc-1) {
			/* port number. listen on this port */
			port = atoi(argv[i]);
		} else {
			fprintf(stderr, "Error parsing command line\n");
			return FAILURE;
		}
	}

	runserver(local_ip, port, file_out);
}
