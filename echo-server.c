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

int runserver(int port, char* ipstr, int echo_locally)
{
	int sockfd = -1;
	int new_fd = -1;
	char *buf = NULL;
	int ret = FAILURE;
	struct in_addr ip;

	if(port == 0)
		port = LISTEN_PORT;

	if(ipstr == NULL) ip.s_addr = htonl(INADDR_ANY);
	else if(1 != inet_pton(AF_INET, ipstr, &ip)) {
		fprintf(stderr, "%s : Not a valid ip", ipstr);
		goto err;
	}

	if(0 > (sockfd = socket(PF_INET, SOCK_STREAM, 0))) {
		fprintf(stderr, "Error creating socket\n");
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
		fprintf(stderr, "listen failed. errno=%d\n", errno);
		goto err;
	}

	/* READBUF_SIZE + 1 for null('\0') character */
	if(NULL == (buf = malloc(READBUF_SIZE+1))) {
		fprintf(stderr, "Memory allocation failed\n");
		goto err;
	}

	while(1) {
		if(ipstr) printf("Listening on %s:%d\n", ipstr, port);
		else printf("Listening on port %d\n", port);

		if(0 > (new_fd = accept(sockfd, NULL, NULL))) {
			fprintf(stderr, "Accept failed. errno=%d\n", errno);
			goto err;
		}

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

		if(i == 0) printf("\nconnection closed\n");
		else {
			fprintf(stderr, "Read error: %d\n", errno);
			goto err;
		}
	}

	err:
	if(sockfd > -1) close(sockfd);
	if(new_fd > -1) close(new_fd);
	if(buf) free(buf);
	return ret;
}

int main(int argc, char* argv[])
{
	int port = 0;
	char *ip = NULL;
	int i = 1;
	while(argc > 1)
	{
		if(argc > 2 && 0 == strcmp(argv[i], "-port")) {
			argc--;
			i++;
			port = atoi(argv[i]);
			argc--;
		}else if(ip == NULL) {
			ip = argv[i];
			i++;
			argc--;
		}else {
			fprintf(stderr, "malformed command line\n");
			return FAILURE;
		}
	}

	return runserver(port, ip, 1/*True*/);
}
