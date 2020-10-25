#include <stdio.h>      /* standard C i/o facilities */
#include <stdlib.h>     /* needed for atoi() */
#include <unistd.h>	    /* defines STDIN_FILENO, system calls,etc */
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>	/* gethostbyname */
#include <string.h>
#include <sys/time.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>

/* master program:

   The following must passed in on the command line:
   hostname of the server (argv[1])
   port number of the server (argv[2])
   request size (argv[3])
   server = rt_client
   */

typedef unsigned long long ticks;

static __inline__ ticks getticks(void)
{
    unsigned a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));

    return (((ticks)a) | (((ticks)d) << 32));
}

#define MAXBUFLEN 10*1024

int main( int argc, char **argv ) 
{
	struct addrinfo hints, *servinfo, *p;
    char buf[MAXBUFLEN];
    int buf_len;
    int n_sent;
    int n_read;
    int rv;

    int sockfd;


	// Make sure we have the right number of command line args 
    if (argc != 4) {
		printf("Usage: %s <server name> <port number> <request size>\n",argv[0]);
		exit(0);
    }

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("rt_master: Problem creating socket\n");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to bind socket\n");
		return 2;
	}


    // Setup non-blocking mode for sending
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
    {
		perror("Unable to set socket to asynchronous mode");
		exit(1);
    }

    /* read everything possible */
    buf_len = atoi(argv[3]);
    if (buf_len == 0 || buf_len > MAXBUFLEN) {
    	fprintf(stderr, "rt_master: could not get a sane buflen (%d) size (0<buflen<%d)\n", buf_len, MAXBUFLEN);
    	exit(1);
    }

#if 0
    struct sched_param schedparam;
    schedparam.sched_priority = 75;
    if (sched_setscheduler(0, SCHED_RR, &schedparam)) {
		perror("Problem setting schedule policy");
		exit(1);
    }
#endif

    ticks send_ticks, send2_ticks, recv_ticks;

    const int num_iters = 100000;
    double rtt = 0.0;
    double sendtime = 0.0;

    int i;
    for (i=0; i<num_iters; i++) {
		send_ticks = getticks();

		while (1) {
			n_sent = sendto(sockfd, buf, buf_len, 0, p->ai_addr, p->ai_addrlen);
			if (n_sent != -1)
				break;
			if (n_sent == -1 && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
				perror("Problem sending data");
				exit(1);
			}
		}

		send2_ticks = getticks();

		//if (n_sent!=buf_len) {
		//printf("Sendto sent %d bytes\n",n_sent);
		//}

		/* Wait for a reply (from anyone) */
		while (1) {
			n_read = recvfrom(sockfd,buf,MAXBUFLEN,0,p->ai_addr, &(p->ai_addrlen));
			if (n_read == -1) {
				if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
					perror("Problem in recvfrom");
					exit(1);
				}
			} else
				break;
		}
		/* record the time after receiving a datagram */
		recv_ticks = getticks();

		rtt += (recv_ticks-send_ticks);
		sendtime += (send2_ticks-send_ticks);
    }

    fprintf(stdout, "%d: %g %g\n", buf_len, (rtt)/num_iters, (sendtime)/num_iters);

    return(0);
}

