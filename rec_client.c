#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <event.h>
#include "g711.c"

#define PORT "3490"
#define BUFSIZE 1024
int sockfd,sock_set_flag = 0;

void sig_handler()          //signal handler
{
    char c;
    printf("Do you really want to exit?[y/n]\n");
    scanf("%c",&c);
    if(c=='y' || c=='Y')
    {
        if(sock_set_flag)
            send(sockfd,"EXIT",4,0);     //send exit message to server
        exit(0);
    }
}

void *get_in_addr(struct sockaddr *sa)   //ipv4 or v6
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static ssize_t loop_write(int fd, const uint8_t *data, size_t size)
{
    ssize_t ret = 0;
    while (size > 0) {
        ssize_t r;
        if ((r = send(fd, data, size, 0)) < 0)
            return r;
        if (r == 0)
            break;
        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }
    return ret;
}



/***************************************************
Global declarations - actually should be inside main 
****************************************************/
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_simple *s = NULL;                    //playback stream
    int error;
    struct addrinfo hints, *servinfo, *p;   //type of addr, server addr, iterator
    int ret_val;
    char ser_addr_str[INET6_ADDRSTRLEN];    //ip address in string
    //struct timeval t1,t2;
/****************************************************
End of Global declarations
****************************************************/

void rec_send(int fd, short event, void *arg)
{
    uint16_t buf[BUFSIZE];
    uint16_t recbuf[BUFSIZE];
        /* Record some data ... */
        //gettimeofday(&t1,NULL);
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0)
        {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            if (s)
                pa_simple_free(s);
            if (sock_set_flag)
                close(sockfd);
            exit(1);
        }
        for(int i=0;i<sizeof(buf)/sizeof(buf[0]);i++)
        {
            recbuf[i]=Snack_Lin2Mulaw(buf[i]);
        }
        if (loop_write(sockfd, recbuf, sizeof(recbuf)) != sizeof(buf))
        {
            fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
            if (s)
                pa_simple_free(s);
            if (sock_set_flag)
                close(sockfd);
            exit(1);
        }
}

int main(int argc,char *argv[])
{
    
	
	if (argc != 2)                          //check if ip address is passed
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
    bzero(&hints,sizeof(hints));           //initialize to zero
    hints.ai_family = AF_UNSPEC;           //any address fam
    hints.ai_socktype = SOCK_STREAM;       //tcp


    signal(SIGINT,sig_handler);            //bind signal handler to Ctrl-C


    /* Create the recording stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error)))
    {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        if (s)
            pa_simple_free(s);
        if (sock_set_flag)
            close(sockfd);
        exit(1);
    }


    /*create a socket and connect it to server*/
    if((ret_val = getaddrinfo(argv[1],PORT,&hints,&servinfo))!=0)   //error in getting address
    {
    	fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(ret_val));
    	return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next)    //find suitable address
    {
    	if((sockfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
    	{
    		perror("client:socket");
    		continue;
    	}

    	if(connect(sockfd,p->ai_addr,p->ai_addrlen) == -1) //connect socket to address
    	{
    		close(sockfd);
    		perror("client:connect");
    		continue;
    	}

    	break;         //if connection made successfully, break
    }

    if(p==NULL)        //if no connection
    {
    	fprintf(stderr, "client:failed to connect\n");
    	return 2;
    }
    sock_set_flag = 1;
    inet_ntop(p->ai_family,get_in_addr((struct sockaddr *)p->ai_addr),ser_addr_str,sizeof(ser_addr_str)); //addr to string
    printf("client:connecting to %s\n", ser_addr_str);

    freeaddrinfo(servinfo); //server info no longer required
    /* connection created
    *
    *
    ********************************
    *All primitive connections done
    ********************************
    *
    * 
    * record and send*/
    struct event ev;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    event_init();
    event_set(&ev,0,EV_PERSIST,rec_send,NULL);
    /*evtimer_set(&ev, say_hello, NULL);*/
    evtimer_add(&ev, &tv);
    event_dispatch();

    return 0;
}