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

int runserver(int port, char* ipstr);

int runserver(int port, char* ipstr)
{
	struct in_addr ip;

	if(port == 0)
		port = LISTEN_PORT;

	if(ipstr == NULL)
		ip.s_addr = htonl(INADDR_ANY);
	else
		if(1 != inet_pton(AF_INET, ipstr, &ip)) {
			fprintf(stderr, "%s : Not a valid ip", ipstr);
			return FAILURE;
		}

	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) {
		fprintf(stderr, "Error creating socket\n");
		return FAILURE;
	}

	struct sockaddr_in s;
	memset(&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	s.sin_addr.s_addr = ip.s_addr;

	if(bind(sockfd, (struct sockaddr *)&s, sizeof(s))) {
		fprintf(stderr, "Bind failed\n");
		return FAILURE;
	}

	if(listen(sockfd, LISTEN_BACKLOG)) {
		fprintf(stderr, "listen failed\n");
		return FAILURE;
	}
	if(ipstr)
		printf("Listening on %s:%d\n", ipstr, port);
	else
		printf("Listening on port %d\n", port);

	int new_fd = accept(sockfd, NULL, NULL);
	if(new_fd < 0) {
		fprintf(stderr, "Accept failed\n");
		return FAILURE;
	}
	
	char *buf = malloc(READBUF_SIZE);
	if(buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		close(new_fd);
		close(sockfd);
		return FAILURE;
	}
	
	int i;
	while((i = recv(new_fd, buf, READBUF_SIZE, 0))>0) {
		buf[i] = '\0';
		fputs(buf, stdout);
	}
	
	if(i == 0) {
		printf("\nconnection closed\n");
		close(new_fd);
		close(sockfd);
		return 0;
	} else {
		fprintf(stderr, "Read error: %d\n", errno);
		close(new_fd);
		close(sockfd);
		return FAILURE;
	}
	/* Should never reach here */
	return 0;
}

int main(int argc, char* argv[])
{
	int i = 1;
	int port = 0;
	char *ip = NULL;
	while(argc > 1)
	{
		if(argc > 2 && 0 == strcmp(argv[i], "-port")) {
			argc--;
			i++;
			port = atoi(argv[i]);
			argc--;
		} else if(ip == NULL) {
			ip = argv[i];
			i++;
			argc--;
		} else {
			fprintf(stderr, "malformed command line\n");
			return FAILURE;
		}
	}

	return runserver(port, ip);
}
