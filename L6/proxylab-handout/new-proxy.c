#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.h"

/* signal handlers */
void sigpipe_handler(int);
void sigint_handler(int);

int main (int argc, char *argv [])
{
	int socket, port, clientlen, connfd;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *haddrp;
	
	/*	Check command line args for errors  */
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port_number>\n", argv[0]);
		exit(1);
	}
	
	port = atoi(argv[1]);
	/* proxy listens to port */
	socket = Open_listenfd(port);
	printf("Socket: %d\n", socket);

}