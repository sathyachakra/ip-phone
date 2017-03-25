#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <event.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define BUFSIZE 1024

void *get_in_addr(struct sockaddr *sa)					//get ipv4 or ipv6 address
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);	//ipv4 addr
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);	//ipv6 addr
}


/***************************************************
Global declarations - actually should be inside main 
****************************************************/
	int sockfd=0,clifd=0;					//socket fd's
	struct addrinfo hints,*servinfo,*p;		//type of addr, server addr, iterator
	struct sockaddr_storage cliaddr;		//client socket
	socklen_t cli_size;						//client size
	int yes=1;								//optval attribute for setsockopt()
	char ser_addr_str[INET6_ADDRSTRLEN];	//ip addr in string format
	int ret_val;
	/* The Sample format to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 2
	};
	pa_simple *s = NULL;					//to store playback stream
	int error;
	//struct timeval t1,t2;
	/*int wr_fp;							//to file*/

	uint8_t buf[BUFSIZE];
	ssize_t num_bytes_recv;
/****************************************************
End of Global declarations
****************************************************/

void recv_play(int fd, short event, void *arg)
{
	if((num_bytes_recv = recv(clifd,buf,sizeof buf,0))==-1)	//receive data
	{
		perror("recv");
		exit(1);
	}
	if(!strcmp(buf,"EXIT"))
	{
		printf("call disconnected\n");
		if(s)
			pa_simple_free(s);
		if (sockfd)
			close(sockfd);
		exit(1);
	}
	else
	{
		/*... and play it */
		if (pa_simple_write(s, buf, (size_t) num_bytes_recv, &error) < 0)
		{
			fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
			if(s)
				pa_simple_free(s);
			if (sockfd)
				close(sockfd);
			exit(1);
		}
		
	}
}

int main(int argc, char const *argv[])
{
	

	memset(&hints,0,sizeof(hints));			//initialize to 0
	hints.ai_family = AF_UNSPEC;			//any address family
	hints.ai_socktype = SOCK_STREAM;		//tcp
	hints.ai_flags = AI_PASSIVE;



    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error)))
    {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));	//display the error
		if(s)
			pa_simple_free(s);
		if(sockfd)
			close(sockfd);
		exit(1);
    }

	/*create socket and bind*/
	if((ret_val = getaddrinfo(NULL,PORT,&hints,&servinfo))!=0)		//get possible addrs
	{
		fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(ret_val));
		return 1;
	}

	for(p=servinfo; p!=NULL ; p= p->ai_next) //find free address among given addresses
	{
		if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1)	//create socket
		{
			perror("server: socket");
			continue;
		}

		if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))==-1)		//set socket options
		{
			perror("setsockopt");
			exit(1);
		}
		//printf("socket:%d\n",sockfd );
		if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1)							//bind socket
		{
			close(sockfd);
			perror("server:bind");
			continue;
		}
		break;
	}

	if(p==NULL)	//if no address free
	{
		fprintf(stderr, "server:failed to bind\n");
		exit(1);
	}


	if(listen(sockfd,BACKLOG)==-1)
	{
		perror("listen");
		exit(1);
	}
	printf("waiting for connections\n");
	/*socket created, bound and listening
	*
	*
	*
	*************************************
	*All primitive connections made with OS
	*************************************
	*
	*
	*
	*
    *receive data*/
    cli_size = sizeof(cliaddr);
	while((clifd = accept(sockfd,(struct sockaddr*)&cliaddr,&cli_size))==-1)	//accept connection
	{
		perror("accept");
		continue;
	}

	inet_ntop(cliaddr.ss_family,get_in_addr((struct sockaddr *)&cliaddr),ser_addr_str, sizeof ser_addr_str);		//change to char string. store in "s"
	printf("server got connection from %s\n", ser_addr_str);

	struct event ev;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 6000;

	event_init();
	event_set(&ev,0,EV_PERSIST,recv_play,NULL);
	/*evtimer_set(&ev, say_hello, NULL);*/
	evtimer_add(&ev, &tv);
	event_dispatch();

	if (pa_simple_drain(s, &error) < 0)
	{
		fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
		if(s)
			pa_simple_free(s);
		if (sockfd)
			close(sockfd);
		exit(1);
	}
}
