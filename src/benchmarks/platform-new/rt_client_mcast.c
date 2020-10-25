/* UDP echo server program */

#include <stdio.h>      /* standard C i/o facilities */
#include <stdlib.h>     /* needed for atoi() */
#include <unistd.h>	/* defines STDIN_FILENO, system calls, etc */
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>  /* gethostbyname */
#include <sched.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <strings.h>

/* this routine echos any messages (UDP datagrams) received */

#define MAXBUF 10*1024
#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

long counter = 0;

void echo_udp(int sd) {
    int n;
    char bufin[MAXBUF];
    struct sockaddr_in remote;
    socklen_t len = 0;

    /* need to know how big address struct is, len must be set before the
     *        call to recvfrom!!! */

    len = sizeof(remote);

	/* read a datagram from the socket (put result in bufin) */
	n=recvfrom(sd, bufin, MAXBUF, 0, (struct sockaddr *)&remote,&len);


	if (n<0) {
	    perror("Error receiving data");
	} else {
		/*printf("GOT %d BYTES\n",n);*/
	    /* Got something, just send it back */
	    sendto(sd, bufin, n, 0, (struct sockaddr *)&remote, len);
	    counter++;
		if (counter % 100000 == 0)
			fprintf(stderr, ".");
	}
}

struct sockaddr* group_address;
void echo_mcast(int sd) {
    int n;
    char bufin[MAXBUF];
    struct sockaddr_in remote;
    socklen_t len = 0;

    /* need to know how big address struct is, len must be set before the
     *        call to recvfrom!!! */

    len = sizeof(remote);

	/* read a datagram from the socket (put result in bufin) */
	n=recvfrom(sd, bufin, MAXBUF, 0, (struct sockaddr *)&remote,&len);

	if (n<0) {
	    perror("Error receiving data");
	} else {
		/*printf("GOT %d BYTES\n",n);*/
	    /* Got something, just send it back */
	    size_t n_sent = sendto(sd, bufin, n, 0, group_address, sizeof(struct sockaddr));

		if (n_sent == -1) {
			perror("Problem echoing: ");
		}
	    counter++;
		if (counter % 100000 == 0)
			fprintf(stderr, ".");
	}
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

void echo_tcp(int sd) {
    int n;
    char bufin[MAXBUF];

	// first, read buffer size
	int sender_buf_len;
	n = recv(sd, (char*)&sender_buf_len, sizeof(int), 0);
	fprintf(stderr, "Got size %d\n", sender_buf_len);

	while (1) {
		/* read a datagram from the socket (put result in bufin) */
		n=recv_all_non_blocking(sd, bufin, sender_buf_len, 0);


		if (n<0) {
			perror("Error receiving data");
			close(sd);
			break;
		} else if (n==0) {
			// orderly shutdown
			close(sd);
			break;
		} else {
			/*printf("GOT %d BYTES\n",n);*/
			/* Got something, just send it back */
			int n_sent = send_all_tcp(sd, bufin, n);
			if (n_sent == -1 && ((errno == EPIPE) || (errno == ENOTCONN) || (errno == ECONNRESET))) {
				close(sd);
				break;
			}
			counter++;
			if (counter % 100000 == 0)
				fprintf(stderr, ".");
		}
	}
}

/* server main routine */

int main(int argc, char** argv) {
    int sockfd, new_fd;
    int yes=1;
    struct sockaddr_in skaddr;
    socklen_t length;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    int rv;


	if (argc < 2) {
		printf("Usage: %s <protocol> [<group addr> <interface>]\n",argv[0]);
		exit(0);
    }

    char *proto = argv[1];
    char *group = NULL;
    char *localname = NULL;

    int using_udp = 0, using_tcp = 0, using_mcast = 0;
    if (!strcmp(proto, "udp"))
		using_udp = 1;
    if (!strcmp(proto, "tcp"))
		using_tcp = 1;
    if (!strcmp(proto, "mcast")) {
		if (argc != 4) {
			printf("Usage: %s <protocol> [<group addr> <interface>]\n",argv[0]);
			fprintf(stderr, "Missing args for mcast\n");
			exit(0);
		}
		using_mcast = 1;
		group = argv[2];
		localname = argv[3];
	}


    if ((sockfd = socket(PF_INET, using_tcp?SOCK_STREAM:SOCK_DGRAM, 0 )) < 0) {
		printf("Problem creating socket\n");
		exit(1);
    }

    /* establish our address
     * address family is AF_INET
     * our IP address is INADDR_ANY (any of our IP addresses)
     * the port number is assigned by the kernel
     */

    skaddr.sin_family = AF_INET;
    skaddr.sin_addr.s_addr = INADDR_ANY; //inet_addr(argv[1]);
    skaddr.sin_port = htons(6364);

	if (using_tcp) {
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		    perror("setsockopt");
		    exit(1);
		}
	}

	struct in_addr iface_addr;
	struct sockaddr_in gaddr;
	if (using_mcast) {
		// Set TTL larger than 1 to enable multicast across routers.
		struct hostent *hent = gethostbyname(group);
		if(hent==NULL) {
		 	printf("%s : unknown group '%s'\n", argv[0], group);
		 	exit(1);
		}

		bzero(&gaddr, sizeof(gaddr));
		gaddr.sin_family = AF_INET;
		gaddr.sin_addr.s_addr = inet_addr(group);
		gaddr.sin_port = htons(6364);
		group_address = (struct sockaddr*)&gaddr;

		iface_addr.s_addr = inet_addr(localname);
		if ((setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &iface_addr, sizeof(iface_addr))) < 0)
		{
	    	perror("setsockopt() failed for binding multicast to interface");
	    	exit(1);
		}

		int error = bind(sockfd, (struct sockaddr*)&gaddr, sizeof(gaddr));
		if (error < 0)
		{
			perror("Unable to name socket");
			exit(1);
		}

		unsigned char i = 20;
		error = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&i, sizeof(i));
		if (error < 0)
		{
			perror("unable to change TTL value");
			exit(1);
		}

		unsigned char loop = 0;
		int rc = setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
		if (rc < 0) {
    		printf("%s : cannot set no loop for multicast in group '%s'",argv[0], inet_ntoa(gaddr.sin_addr));
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
	}

 	if (!using_mcast) {
 		if (bind(sockfd, (struct sockaddr *) &skaddr, sizeof(skaddr))<0) {
			close(sockfd);
			printf("Problem binding\n");
			exit(0);
    	}
    }

    if (using_tcp) {
		if (listen(sockfd, BACKLOG) == -1) {
			perror("listen");
			exit(1);
		}
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    /* find out what port we were assigned and print it out */
    length = sizeof( skaddr );
    if (getsockname(sockfd, (struct sockaddr *) &skaddr, &length)<0) {
		printf("Error getsockname\n");
		exit(1);
    }

    /* port number's are network byte order, we have to convert to
     *      host byte order before printing !
     *        */
    printf("The server %s port number is %d\n",proto, ntohs(skaddr.sin_port));

    while (1) {
		if (using_tcp) {
			sin_size = sizeof their_addr;
			new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
			if (new_fd == -1) {
				perror("accept");
				if (errno == EINVAL)
					exit(1);
				continue;
			}

			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *)&their_addr),
					s, sizeof s);
			// printf("server: got connection from %s\n", s);

			if (!fork()) { // this is the child process
				close(sockfd); // child doesn't need the listener
				echo_tcp(new_fd);
				exit(0);
			}
			close(new_fd);  // parent doesn't need this
		} else if (using_udp) {
			/* Go echo every datagram we get */
			echo_udp(sockfd);
		} else if (using_mcast) {
			/* Go echo every datagram we get, over mcast */
			echo_mcast(sockfd);
		}
    }
    return(0);
}

