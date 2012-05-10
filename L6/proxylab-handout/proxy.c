#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csapp.h"

#define DEFAULT_PORTNO 80

#define DEBUG 1

/* Caching Variables */
#define MAXIMUM_OBJECT_SIZE (100<<10)
#define MAXIMUM_SIZED_CACHE (1<<20)
int list_length = 0;

typedef struct cache_obj_t
{
	struct cache_obj_t *prev;
	struct cache_obj_t *next;
	char *cache_data;
	time_t time;
	int size_of_cache;
	char uri[MAXLINE];
}cache_obj;

cache_obj *head = NULL;

/* Semaphores */
sem_t connection_sem; //For connection thread
sem_t cache_sem; //For cache thread

/* Thread Stuff */
pthread_rwlock_t lock;

/* signal handlers */
void sigpipe_handler(int);
void sigint_handler(int);

/* My helper functions */
void run(int connfd);
int uri_parser(char *uri, char *host, int *port, char *args);
void err_msg(char *err_message);
/*int connClient( rio_t *rio, char *hostname, int port, char *args);*/
int connClient( rio_t *rio, char *hostname, int port, char  *args, char *uri, int connfd);
void *thread_handler(void *vargp);

void add_cache(char *cache_data, char *uri, int size);
void fill_it_up(cache_obj *temp, char *data, char *uri, int size);
int remove_cache();
cache_obj *find_cache(char * uri);

/*
 *	argv = cmd <port>
 */
int main (int argc, char *argv [])
{
	if(DEBUG){ printf("<main>:Proxy started...\n"); }
	int listen_fd, port;
	int connfd, clientlen;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *haddrp;

	pthread_t threadID;

	/*	Check command line args for errors  */
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port_number>\n", argv[0]);
		exit(1);
	}

	/* Initialize semaphores (cache and connection) */
	Sem_init(&connection_sem, 0, 1);
	Sem_init(&cache_sem, 0, 1);

  if(DEBUG){ printf("<main>: doing some thread stuff\n"); }
	pthread_rwlock_init(&lock, 0);
	if(DEBUG){ printf("<main>: done\n"); }

	/*	Install Signals	*/
	Signal(SIGPIPE, sigpipe_handler);
	Signal(SIGINT, sigint_handler);

	/*	Get the port number and socket number */
	port = atoi(argv[1]);
	/* proxy listens to port */
	listen_fd = Open_listenfd(port);

	while(1)
	{
		clientlen = sizeof(clientaddr);
		if( (connfd = Accept(listen_fd, (SA*) &clientaddr, &clientlen)) < 0)
		{
			err_msg("<main>: Accept Failed");
		}

		
		hostp = Gethostbyaddr( (const char *) &clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
		haddrp = inet_ntoa(clientaddr.sin_addr);

		printf("Server connected to %s (%s)\n", hostp->h_name, haddrp);

		/* Does the proxy stuff - process user input */
		Pthread_create(&threadID, NULL, thread_handler, (void *) connfd);

		if(DEBUG){ printf("<main>: Finished thread create\n"); }
		
		/* Before multi-threads and shit 
		run(connfd);
		Close(connfd); */
	}

	return 0;
}

void run(int connfd)
{
	rio_t connRio, clientRio;
	char lineBuffer[MAXLINE];
	char cmd[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], uri_args[MAXLINE];
	int port, buffer_length, realloc_length;
	int clientfd;
	int rio_read_test;

	char *cache_data = NULL;
	int cache_data_length = 0;
	int write_to_cache = 1;

	lineBuffer[0] = '\0';
	Rio_readinitb(&connRio, connfd);

	rio_read_test = rio_readlineb(&connRio, lineBuffer, MAXLINE);
	/* Testing if readlineb worked correctly */
	if( rio_read_test == 0 )
	{
		err_msg("EOF error");
	}
	else if(rio_read_test < 0)
	{
		return;
	}

	/* Extracting method, uri and HTTP version from lineBuffer */
	sscanf(lineBuffer, "%s %s %s", cmd, uri, version);

	if(DEBUG)
	{
		printf("command: %s\n", cmd);
		printf("uri: %s\n", uri);
	}

	/* if method is not GET, get the fuck out of here */
	if( strcasecmp(cmd, "GET") != 0 )
	{
		err_msg("This function doesn not exist!");
		return;
	}

	/* uri_parser extracts the hostname, port and args from the URI */
	if( uri_parser(uri, hostname, &port, uri_args) )
	{
		if( (clientfd = connClient(&connRio, hostname, port, uri_args, uri, connfd)) == -10)
		{
			/* if object found in cache */
			return;
		}
		else if(clientfd != 0)
		{
			if( clientfd < 0 )
			{
				err_msg("Couldn't connect to server");
			}

			Rio_readinitb(&clientRio, clientfd);

			/* Loop runs while theres input */
			while( (buffer_length = rio_readnb(&clientRio, lineBuffer, MAXLINE)) > 0 )
			{
				if(DEBUG)
				{
					printf("buffer: %s\n", lineBuffer);
					printf("Length: %d\n", buffer_length);
				}

				/* if nothing is written */
				if( rio_writen(connfd, lineBuffer, buffer_length) < 0)
				{
					write_to_cache = 0;
					break;
				}

				if(write_to_cache)
				{
					/* expands cache to fit next object in */
					realloc_length = buffer_length + cache_data_length;
					cache_data = Realloc(cache_data, realloc_length);
					memcpy(cache_data + cache_data_length, lineBuffer, buffer_length);
					cache_data_length = realloc_length;

					/* if new object is > max object size */
					if( (cache_data_length + sizeof(cache_obj)) > MAXIMUM_OBJECT_SIZE )
					{
						if(DEBUG){ err_msg("<run>: Object too big"); }

						/* Doesn't write new object ot cache */
						write_to_cache = 0;

						if( cache_data != NULL )
						{
							free(cache_data);
						}

						cache_data = NULL;
						cache_data_length = 0;
					}
				}

				if(write_to_cache)
				{
					if(DEBUG){ printf("<run>: Writing to cache...\n"); }
					add_cache(cache_data, uri, cache_data_length);
				}
			}
		}
		Close(clientfd);
	}
	else
	{
		err_msg("URI parsing failed");
		return;
	}
}

/* Ignore sigpipe signal */
void sigpipe_handler(int signal)
{
  fprintf(stderr,"SIGPIPE %d received and ignored\n", signal);
}

/* EXIT proxy */
void sigint_handler(int signal)
{
	exit(signal);
}

int uri_parser(char *uri, char *host, int *port, char *args)
{
	int uri_traverser = 0, temp_traverser = 0;
	char temp[MAXLINE];
	char temp2[MAXLINE];
	char *protocol = "http://";

	if(uri==NULL)
	{
		err_msg("Invalid URI : not found");
		return 0;
	}

	/* Copy protocol into temp from URI */
	strncpy(temp, uri, 7);

	/* Advance URI's iterator */
	uri_traverser = 7;
	
	/* Checks that protocol in the URI is HTTP */
	if( strncasecmp(temp, protocol, 7) != 0 )
	{
		err_msg("This proxy only supports HTTP");
		return 0;
	}

	/*	Alternate (quicker?) way to test temp for protocol

  for(i = 0; i<7; i++)
  {
    if(temp[i]!=protocol[i])
    {
      err_msg("Only http is supported");
      return 0;
    }
  }

	*/

	/* Extract hostname from URI */
  while(uri[uri_traverser]!='/' && uri[uri_traverser] != ':')
  {
    host[temp_traverser++] = uri[uri_traverser++];
  }
  host[temp_traverser]='\0';

  printf("Hostname: %s\n", host);

	/* Loads port number if provided, else sets to DEFAULT_PORTNO */
  if(uri[uri_traverser] ==':')
  {
    temp_traverser = 0;
    uri_traverser++;
    while(uri[uri_traverser] !='/')
    {
      temp2[temp_traverser] = uri[uri_traverser];
      temp_traverser++;
      uri_traverser++;
    }
    *port = atoi(temp2);
  }
  else
  {
    *port = DEFAULT_PORTNO;
  }

	/* Reset temp array's iterator */
  temp_traverser = 0; 

	/* Copies arguments from URI into args */
  while(uri[uri_traverser] !='\0')
  {
    args[temp_traverser++] = uri[uri_traverser++];
  }
  args[temp_traverser] ='\0';

	if(DEBUG){ printf("Done parsing the uri\n"); }
  return 1;
}

int connClient( rio_t *rio, char *hostname, int port, char  *args, char *uri, int connfd)
{
    int clientfd = Open_clientfd(hostname, port);
    char header[MAXLINE], buffer[MAXLINE];
    int isHost = 0;

		cache_obj *cache1;

		if(clientfd<0)
		{
			return 0;

		}

    strcpy(header, "GET ");
    strcat(header, args);
    strcat(header, " HTTP/1.0\r\n");

		cache1 = find_cache(uri);

		if(cache1!=NULL)
		{
 			if(DEBUG){ printf("<connClient>: Found it in cache\n"); }
			do
			{
				if(DEBUG){ printf("Going to rio_readlineb\n"); }
				if(rio_readlineb(rio, header, MAXLINE) <=0)
				{
					break;
				}
			}while(strcmp(header, "\r\n"));
			
			rio_writen(connfd, cache1->cache_data, cache1->size_of_cache);
			if(DEBUG){ printf("<connClient>: Returning -10\n"); }
			return -10;
		}

  	Rio_writen(clientfd, header, strlen(header));

  	while( (rio_readlineb(rio, header, MAXLINE) > 0) && (strcmp(header, "\r\n")) )
		{
			/* Check the header for connection type */
  	  strncpy(buffer, header, 17); /* Copy longest connection type into buffer */
  	  buffer[16] = '\0';
	
  	  /* if "Proxy-Connection" */
  	  if( strcasecmp(buffer, "Proxy-Connection") == 0 )
  	  {
				strcpy(header, "Proxy-Connection: close\r\n");
  	    Rio_writen(clientfd, header, strlen(header));
	
				continue;
  	  }
 	
  	  buffer[10] = '\0'; /* Shorten buffer for next type */
	
  	  if( strcasecmp(buffer, "Connection") == 0 )
  	  {
  	    strcpy(buffer, "Connection: close\r\n");
  	    Rio_writen(clientfd, header, strlen(header));
				continue;
  	  }
	
  	  if( strcasecmp(buffer, "Keep-Alive") == 0 )
  	  {
  	    continue;
  	  }

  	  if( !isHost )
  	  {
  	    buffer[4] = '\0'; /* Shorten buffer for next type */
  	    if( strcasecmp(buffer, "Host") == 0 )
  	    {
					isHost = 1;
  	      Rio_writen(clientfd, header, strlen(header));
					continue;
  	    }
  	   }
  	   Rio_writen(clientfd, header, strlen(header));

  	  }

  	  if( !isHost )
  	  {
				strcpy(header, "Host: ");
  	    strcat(header, hostname);
  	    strcat(header, "\r\n");
	
  	    printf("Header: %s\n", header);
  	    Rio_writen(clientfd, header, strlen(header));

  	  }

  	  strcpy(header, "\r\n\n");
  	  Rio_writen(clientfd, header, strlen(header));

 	  return clientfd;
}
void err_msg(char *err_message)
{
  printf(err_message);
  printf("\n");
}

/* 	Caching Functions	*/

void add_cache(char *cache_data, char *uri, int size)
{
  cache_obj *temp = NULL;
	if(DEBUG){ printf("<add_cache>: Adding to cache...\n"); }
  
	pthread_rwlock_wrlock(&lock);
	P(&cache_sem);

  while(strlen(cache_data)+sizeof(cache_obj) + list_length > MAXIMUM_SIZED_CACHE)
  {
    remove_cache();
  }
  fill_it_up(temp, cache_data, uri, size);
  list_length = list_length+strlen(cache_data)+sizeof(cache_obj);

	V(&cache_sem);
	pthread_rwlock_unlock(&lock);
}

void fill_it_up(cache_obj *temp, char *data, char *uri, int size)
{
  if(head == NULL)
  {
    head = malloc(sizeof(cache_obj));
    head->prev = NULL;
    head->next = NULL;
    head->cache_data = data;
    head->time = time(NULL);
    strcpy(head->uri, uri);
    head->size_of_cache = size;
  }
  else
  {
    
    temp = malloc(sizeof(cache_obj));
    temp->prev = NULL;
    temp->next = head;
    temp->cache_data = data;
    temp->time = time(NULL);
    strcpy(temp->uri, uri);
    temp->size_of_cache = size;
  }
}

int remove_cache()
{
  cache_obj *temp = head;
  cache_obj *node_to_del = head;
  if(temp ==NULL)
  {
    return 0;
  }

  while(temp != NULL)
  {
    if(difftime(node_to_del->time, temp->time) > 0)
    {
      node_to_del = temp;
    }
    temp = temp->next;
  }

  if(node_to_del == head)	/* if node to delete is the head */
  {
    head = head->next;
		if(head != NULL)
		{
			head->prev = NULL;
		}
  }
	else if( node_to_del->next == NULL ) /* if the node to delete is the tail */
	{
		(node_to_del->prev)->next = NULL;
	}
	else	/* if node to delete is in the middle of the list */
	{
	  node_to_del->next->prev = node_to_del->prev;
   	node_to_del->prev->next = node_to_del->next;  	
	}
  
  if(node_to_del != NULL)
  {
    if(node_to_del->cache_data != NULL)
    {
      free(node_to_del->cache_data);
    }
    free(node_to_del);
  }  
  return 1;
}

//To find a cache with a matching URI
cache_obj *find_cache(char * uri)
{
  if(head == NULL)
  {
    return 0;
  }

  cache_obj *temp = NULL;
	pthread_rwlock_rdlock(&lock);

  temp = head;
  while(temp !=NULL)
  {
    if(strcmp(temp->uri, uri)==0)
    {
 			if(DEBUG){ printf("<find_cache>: Found it\n"); }
      break;
    }
    temp = temp->next;
  }

  if(temp !=NULL)
  {
		P(&cache_sem);
    temp->time = time(NULL);
		V(&cache_sem);
  }
  pthread_rwlock_unlock(&lock);

  return temp;
}

void *thread_handler(void *vargp)
{
	if(DEBUG){ printf("In thread handler\n"); }
  int connfd = (int) vargp;
  Pthread_detach(pthread_self());
	if(DEBUG){ printf("Detached thread\n"); }
  /*if(DEBUG){ printf("Attempting to free vargp...\n"); }
	free(vargp);
  if(DEBUG){ printf("Freed vargp\n"); }*/
	if(DEBUG){ printf("Starting run...\n"); }
  run(connfd);
	if(DEBUG){ printf("Done running\n"); }
	if( close(connfd) < 0 )
	{
		printf("Error closing connfd\n");
	}
	return NULL;
}
