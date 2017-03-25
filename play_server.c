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

#define MAXDATASIZE 1024

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define BUFSIZE 1024

static ssize_t loop_write(int fd, const uint8_t *data, size_t size) {
    ssize_t ret = 0;
    while (size > 0) {
        ssize_t r;
        if ((r = write(fd, data, size)) < 0)
            return r;
        if (r == 0)
            break;
        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }
    return ret;
}

void *get_in_addr(struct sockaddr *sa)					//get ipv4 or ipv6 address
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);	//ipv4 addr
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);	//ipv6 addr
}

int main(int argc, char const *argv[])
{
	int sockfd,clifd;						//socket fd's
	struct addrinfo hints,*servinfo,*p;		//type of addr, server addr, iterator
	struct sockaddr_storage cliaddr;		//client socket
	socklen_t cli_size;						//clint size
	//struct sigaction sa;
	int yes=1;
	char ser_addr_str[INET6_ADDRSTRLEN];
	int ret_val;
	//char buffer[250];						//buffer to store data

	memset(&hints,0,sizeof(hints));			//initialize to 0
	hints.ai_family = AF_UNSPEC;			//any address family
	hints.ai_socktype = SOCK_STREAM;		//tcp
	hints.ai_flags = AI_PASSIVE;

/* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_simple *s = NULL;
    int ret = 1;
    int error;
    
    int wr_fp;

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

	//create socket and bind
	if((ret_val = getaddrinfo(NULL,PORT,&hints,&servinfo))!=0)		//error in getting addr
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
		if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1)							//vind socket
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
	//socket created, bound and listening



	//*************
	//*************



	
    //receive data
    cli_size = sizeof(cliaddr);
	while((clifd = accept(sockfd,(struct sockaddr*)&cliaddr,&cli_size))==-1)	//accept connection
	{
		perror("accept");
		continue;
	}

	inet_ntop(cliaddr.ss_family,get_in_addr((struct sockaddr *)&cliaddr),ser_addr_str, sizeof ser_addr_str);		//change to char string. store in "s"
	printf("server got connection from %s\n", ser_addr_str);
	if ((wr_fp = open("recording.raw", O_CREAT|O_WRONLY)) < 0) {
        fprintf(stderr, __FILE__": open() failed: %s\n", strerror(errno));
        goto finish;
    }
    if (dup2(wr_fp, STDOUT_FILENO) < 0) {
        fprintf(stderr, __FILE__": dup2() failed: %s\n", strerror(errno));
        goto finish;
    }
    close(wr_fp);
for (;;)
    {
		uint8_t buf[BUFSIZE];
		ssize_t num_bytes_recv;
		
		while(1)
		{
			if((num_bytes_recv = recv(clifd,buf,sizeof buf,0))==-1)	//receive data
			{
				perror("recv");
				exit(1);
			}

			/* And write it to STDOUT */
			if (loop_write(STDOUT_FILENO, buf, sizeof(buf)) != sizeof(buf))
			{
				fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
				goto finish;
			}
		}

#if	0	//... and play it 
		if (pa_simple_write(s, buf, (size_t) num_bytes_recv, &error) < 0)
		{
			fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
			goto finish;
		}
#endif
	}


finish:
	if(s)
		pa_simple_free(s);
	close(sockfd);
}