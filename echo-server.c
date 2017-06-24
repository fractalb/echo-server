#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define READBUF_SIZE 20
#define LISTEN_BACKLOG 2
#define LISTEN_PORT 8001
#define FAILURE (-1)

int runserver(int port, char* ipstr, int echo_locally);
void print_client_details(struct sockaddr_in *clnt_addr);

void print_client_details(struct sockaddr_in *clnt_addr)
{
	char buf[INET_ADDRSTRLEN];

	if(NULL == inet_ntop(AF_INET, &(clnt_addr->sin_addr), buf, INET_ADDRSTRLEN)) {
		fprintf(stderr, "Connection established, but unable to get the client details.\n");
		return;
	}
	printf("Accepted a connection from %s:%d\n", buf, ntohs(clnt_addr->sin_port));
	return;
}

int runserver(int port, char* ipstr, int echo_locally)
{
	int sockfd = -1;
	int new_fd = -1;
	char *buf = NULL;
	struct in_addr ip;

	if(port == 0)
		port = LISTEN_PORT;

	if(ipstr == NULL) ip.s_addr = htonl(INADDR_ANY);
	else if(1 != inet_pton(AF_INET, ipstr, &ip)) {
		fprintf(stderr, "%s : Not a valid ip", ipstr);
		goto err;
	}

	if(0 > (sockfd = socket(PF_INET, SOCK_STREAM, 0))) {
		fprintf(stderr, "Unable to create a socket. errno=%d\n", errno);
		goto err;
	}

	struct sockaddr_in s;
	memset(&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	s.sin_addr.s_addr = ip.s_addr;

	if(bind(sockfd, (struct sockaddr *)&s, sizeof(s))) {
		fprintf(stderr, "Bind failed. errno=%d\n", errno);
		goto err;
	}

	if(listen(sockfd, LISTEN_BACKLOG)) {
		fprintf(stderr, "Listen failed. errno=%d\n", errno);
		goto err;
	}

	/* READBUF_SIZE + 1 for null('\0') character */
	if(NULL == (buf = malloc(READBUF_SIZE+1))) {
		fprintf(stderr, "Memory allocation failed\n");
		goto err;
	}

	struct sockaddr_in clnt_addr;
	int clnt_len;
	while(1) {
		if(ipstr) printf("Listening on %s:%d\n", ipstr, port);
		else printf("Listening on port %d\n", port);

		clnt_len = sizeof(clnt_addr);
		memset(&clnt_addr, 0 , clnt_len);

		if(0 > (new_fd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_len))) {
			fprintf(stderr, "Accept failed. errno=%d\n", errno);
			goto err;
		}

		print_client_details(&clnt_addr);

		int i, j, k;
		while((i = recv(new_fd, buf, READBUF_SIZE, 0))>0) {
			if(echo_locally) {
				buf[i] = '\0';
				fputs(buf, stdout);
			}
			k = 0;
			while(i != k) {
				if(0 > (j = send(new_fd, buf+k, i-k, 0))) {
					fprintf(stderr, "Send failure. errno=%d\n", errno);
					goto err;
				}
				k += j;
			}
		}

		if(i == 0) printf("\nConnection closed\n");
		else {
			fprintf(stderr, "Read error: %d\n", errno);
			goto err;
		}
	}

	err:
	if(sockfd > -1) close(sockfd);
	if(new_fd > -1) close(new_fd);
	if(buf) free(buf);
	return FAILURE;
}

int main(int argc, char* argv[])
{
	int port = 0;
	char *ip = NULL;
	int echo_locally = 0/*False*/;
	int i = 1;
	while(i < argc)
	{
		if(0 == strcmp(argv[i], "-e")) {
		/* Echo locally */
			i++;
			echo_locally = 1/*True*/;
		}else if(0 == strcmp(argv[i], "-p")) {
		/* port number. listen on this port */
			i++;
			port = atoi(argv[i]);
			i++;
		}else if(ip == NULL) {
			ip = argv[i];
			i++;
		}else {
			fprintf(stderr, "malformed command line\n");
			return FAILURE;
		}
	}

	return runserver(port, ip, echo_locally);
}
