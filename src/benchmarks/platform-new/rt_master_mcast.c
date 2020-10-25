#include <stdio.h>      /* standard C i/o facilities */
#include <stdlib.h>     /* needed for atoi() */
#include <unistd.h>	    /* defines STDIN_FILENO, system calls,etc */
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <netinet/tcp.h>
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>	/* gethostbyname */
#include <string.h>
#include <sys/time.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>

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

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int send_all_tcp(int s, void *buf, size_t len)
{
	size_t total = 0; // how many bytes we've sent
	size_t bytesleft = len; // how many we have left to send
	size_t n = -1;

	while (total < len)
	{
		n = send(s, buf + total, bytesleft, 0);
		if (n == -1)
		{
			continue;
		}
		total += n;
		bytesleft -= n;
	}

	return n == -1 ? -1 : total; // return -1 on failure, total on success
}

int recv_all_non_blocking(int s, void *buf, size_t len, int flags)
{
	int len_tmp = 0;
	int n;
	do
	{
		if (len_tmp > 0)
		{
			//fprintf(stderr, "len_tmp = %d\n", len_tmp);
		}
		n = recv(s, &(((char *) buf)[len_tmp]), len - len_tmp, 0);
		if (n == -1)
		{
			//      perror("Recv_all: ");
			//      return -1;
			continue;
		}
		len_tmp = len_tmp + n;
	} while (len_tmp < len);
	return len_tmp;
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
    if (argc < 5) {
		printf("Usage: %s <protocol> <server name> <port number> <request size> [<bindToAddr>]\n",argv[0]);
		exit(0);
    }

    char *proto = argv[1];
    char *localname = NULL;

    int using_udp = 0, using_tcp = 0, using_mcast = 0;
    if (!strcmp(proto, "udp"))
    	using_udp = 1;
    if (!strcmp(proto, "tcp"))
    	using_tcp = 1;
    if (!strcmp(proto, "mcast")) {
    	using_mcast = 1;
    	if (argc != 6) {
			printf("Usage: %s <protocol> <server name> <port number> <request size> [<bindToAddr>]\n",argv[0]);
			fprintf(stderr, "Missing args for mcast (bind addr)\n");
			exit(0);
    	}
    	localname = argv[5];
    }

	char *group = argv[2];
	struct in_addr iface_addr;
	struct sockaddr_in gaddr;
	struct sockaddr* group_address;

    if (using_udp || using_tcp) {
    	memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_INET;
		hints.ai_socktype = using_udp?SOCK_DGRAM:SOCK_STREAM;

		if ((rv = getaddrinfo(argv[2], argv[3], &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
			return 1;
		}

		// loop through all the results and make a socket
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("net_master: Problem creating socket\n");
				continue;
			}

			if (using_tcp) {
				if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			    	close(sockfd);
			    	perror("net_master: connect");
			    	continue;
				}
			}

			break;
		}

		if (p == NULL) {
			fprintf(stderr, "talker: failed to bind socket\n");
			return 2;
		}


    } else if (using_mcast) {
    	fprintf(stderr, "Setting up mcast\n");
    	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP )) < 0) {
			printf("Problem creating socket\n");
			exit(1);
    	}

		bzero(&gaddr, sizeof(gaddr));
		gaddr.sin_family = AF_INET;
		gaddr.sin_addr.s_addr = inet_addr(group);
		gaddr.sin_port = htons(6364);
		group_address = (struct sockaddr*)&gaddr;

		int flag_on = 1;
		/* set reuse port to on to allow multiple binds per host */
		if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) < 0) {
	       	perror("setsockopt() failed");
	        exit(1);
		}


		// Set TTL larger than 1 to enable multicast across routers.
		unsigned char i = 20;
		int error = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&i, sizeof(i));
		if (error < 0)
		{
			perror("unable to change TTL value");
			exit(1);
		}

		iface_addr.s_addr = inet_addr(localname);
		if ((setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &iface_addr, sizeof(iface_addr))) < 0)
		{
	    	perror("setsockopt() failed for binding multicast to interface");
	    	exit(1);
		}

		error = bind(sockfd, (struct sockaddr*)&gaddr, sizeof(gaddr));
		if (error < 0)
		{
			perror("Unable to name socket");
			exit(1);
		}

		// explicitely enable loopback
		unsigned char loop = 0;
		if ((setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop))) < 0)
		{
	    	perror("setsockopt() failed for enabling loop");
	    	exit(1);
		}

		// Set TTL larger than 1 to enable multicast across routers.
		unsigned char ttl = 20;
		if ((setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl))) < 0)
		{
			perror("unable to change TTL value");
			exit(1);
		}

		struct ip_mreq multicast_req;
		multicast_req.imr_multiaddr.s_addr = inet_addr(group);
		multicast_req.imr_interface.s_addr = inet_addr(localname);
		if ((setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &multicast_req,
						sizeof(multicast_req))) < 0)
		{
			perror("Unable to join group");
			exit(1);
		}
	} else {
    	fprintf(stderr, "rt_master: protocol is not any of 'tcp', 'udp', 'mcast'\n");
    	exit(1);
    }

    // Setup non-blocking mode for sending
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1)
    {
		perror("Unable to set socket to asynchronous mode");
		exit(1);
    }

	/* Disable the Nagle (TCP No Delay) algorithm */
	if (using_tcp) {
		int flag = 1;
		if ((setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int))) < 0) {
			perror("Unable to disable nagle");
			exit(1);
		}
	}

    /* read everything possible */
    buf_len = atoi(argv[4]);
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
    if (using_tcp) {
    	n_sent = send(sockfd, (char*)&buf_len, sizeof(buf_len), 0);
		if (n_sent == -1 && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
			fprintf(stderr, "Problem sending buffer size\n");
			perror("Boh");
			exit(1);
		}
    }
    for (i=0; i<num_iters; i++) {
		send_ticks = getticks();

		while (1) {
			// sending part
			if (using_udp)
				n_sent = sendto(sockfd, buf, buf_len, 0, p->ai_addr, p->ai_addrlen);
			else if (using_mcast)
				n_sent = sendto(sockfd, buf, buf_len, 0, group_address, sizeof(struct sockaddr));
			else if (using_tcp)
				n_sent = send_all_tcp(sockfd, buf, buf_len);

			if (n_sent != -1)
				break;
			if (n_sent == -1 && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
				struct in_addr ihate = (((struct sockaddr_in*)group_address)->sin_addr);
				fprintf(stderr, "%s: cannot send data to group %s\n", argv[0], inet_ntoa(ihate));
				fprintf(stderr, "%d %p %d\n", sockfd, buf, buf_len);
				perror("Problem sending data");
				exit(1);
			}
		}

		send2_ticks = getticks();

		if (n_sent!=buf_len) {
		printf("Sendto sent %d bytes\n",n_sent);
		}

		/* Wait for a reply (from anyone) */
		while (1) {
			// receiving part
			if (using_udp)
				n_read = recvfrom(sockfd, buf, buf_len, 0, p->ai_addr, &(p->ai_addrlen));
			else if (using_mcast)
				n_read = recvfrom(sockfd, buf, buf_len, 0, NULL, NULL);
			else if (using_tcp)
				n_read = recv_all_non_blocking(sockfd, buf, buf_len, 0);

			if (n_read == -1) {
				if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
					perror("Problem in recvfrom");
					exit(1);
				}
			} else
				break;
		}

		if (n_read != buf_len) {
		printf("recv returned %d bytes\n", n_read);
		}
		/* record the time after receiving a datagram */
		recv_ticks = getticks();

		rtt += (recv_ticks-send_ticks);
		sendtime += (send2_ticks-send_ticks);
    }

	if (using_tcp)
		shutdown(sockfd, SHUT_RDWR);
	close(sockfd);	
    fprintf(stdout, "%d: %g %g\n", buf_len, (rtt)/num_iters, (sendtime)/num_iters);

    return(0);
}

