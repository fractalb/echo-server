#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define READBUF_SIZE 20
#define LISTEN_BACKLOG 2
#define LISTEN_PORT 8001

int main()
{
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	if(sockfd < 0) {
		fprintf(stderr, "Error creating socket\n");
		return -1;
	}

	struct sockaddr_in s;
	memset(&s, 0, sizeof(s));
	s.sin_family = AF_INET;
	s.sin_port = htons(LISTEN_PORT);
	s.sin_addr.s_addr = INADDR_ANY;

	if(bind(sockfd, (struct sockaddr *)&s, sizeof(s))) {
		fprintf(stderr, "Bind failed\n");
		return -1;
	}

	if(listen(sockfd, LISTEN_BACKLOG)) {
		fprintf(stderr, "listen failed\n");
		return -1;
	}

	int new_fd = accept(sockfd, NULL, NULL);
	if(new_fd < 0) {
		fprintf(stderr, "Accept failed\n");
		return -1;
	}
	
	char *buf = malloc(READBUF_SIZE);
	if(buf == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		close(new_fd);
		close(sockfd);
		return -1;
	}
	
	int i;
	while((i = recv(new_fd, buf, READBUF_SIZE, 0))>0) {
		buf[i] = '\0';
		fputs(buf, stdout);
	}
	
	if(i == 0) {
		printf("connection closed\n");
		close(new_fd);
		close(sockfd);
		return 0;
	} else {
		fprintf(stderr, "Read error: %d\n", errno);
		close(new_fd);
		close(sockfd);
		return -1;
	}
	/* Should never reach here */
	return 0;
}
