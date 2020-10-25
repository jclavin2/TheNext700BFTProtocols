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
#include <pthread.h>

#include "Digest.h"
#include "MD5.h"
#include "crypt.h"
#include "rabin.h"
#include "umac.h"

#define MAX_MSG_SIZE 64*1024
#define MSG_SIZE(x) (x+NUM_PRINCIPALS*(UNonce_size+UMAC_size))

bool opt_macs = true;

typedef unsigned long long ticks;

static __inline__ ticks getticks(void)
{
    unsigned a, d;
    asm("cpuid");
    asm volatile("rdtsc" : "=a" (a), "=d" (d));

    return (((ticks)a) | (((ticks)d) << 32));
}

void comp_digest(char *src, size_t size, Digest& d) __attribute__ ((noinline));
void comp_digest(char *src, size_t size, Digest& d)
{
	MD5_CTX context;
	MD5Init(&context);
	MD5Update(&context, src, size); 
	MD5Final(d.udigest(), &context);
}

const unsigned int UMAC_size = 8;
const unsigned int UNonce_size = sizeof(long long);
const unsigned int MAC_size = UMAC_size + UNonce_size;

const unsigned int Nonce_size = 16;
const unsigned int Nonce_size_u = Nonce_size/sizeof(unsigned);
const unsigned int Key_size = 16;
const unsigned int Key_size_u = Key_size/sizeof(unsigned);


static long long umac_nonce;
inline static long long new_umac_nonce()
{
	return ++umac_nonce;
}

#define NUM_PRINCIPALS 11
// this way we can have 10 MACs per message

umac_ctx_t* ctxs[NUM_PRINCIPALS];

int num_principals;
void setup_umac()
{
	num_principals = NUM_PRINCIPALS;
	for (int id=0; id<NUM_PRINCIPALS; id++) {
		ctxs[id] = (umac_ctx_t*)malloc(num_principals*sizeof(umac_ctx_t*));

		umac_ctx_t *ctxs_t = ctxs[id];

		for (int index=0; index < num_principals; index++)
		{
			int temp = id*1000 + index;

			unsigned k[Key_size_u];
			for (unsigned int j = 0; j<Key_size_u; j++)
			{
				k[j] = temp + j;
			}
			*ctxs_t = umac_new((char*)k);
			ctxs_t++;
		}
	}
}


void gen_mac(const char *src, unsigned src_len, 
			 char *dst, int src_pid, int dest_pid, const char *unonce) __attribute__ ((noinline));
void gen_mac(const char *src, unsigned src_len, 
			 char *dst, int src_pid, int dest_pid, const char *unonce) {
	umac_ctx_t ctx = ctxs[src_pid][dest_pid];
	umac(ctx, (char *)src, src_len, dst, (char *)unonce);
	umac_reset(ctx);
}

bool verify_mac(int from, int to, const char *src, unsigned src_len,
		const char *mac, const char *unonce) __attribute__ ((noinline));
bool verify_mac(int from, int to, const char *src, unsigned src_len,
		const char *mac, const char *unonce)
{
	umac_ctx_t ctx = ctxs[from][to];
	// Do not accept MACs sent with uninitialized keys.
	if (ctx == 0)
	{
		fprintf(stderr,
				"zl_Principal::verify_mac: MACs sent with uninitialized keys\n");
		return false;
	}

	char tag[20];
	umac(ctx, (char *)src, src_len, tag, (char *)unonce);
	umac_reset(ctx);

	bool toRet = !memcmp(tag, mac, UMAC_size);

	return toRet;
}

void gen_auth(int id, char *src, unsigned len, char *dest) __attribute__ ((noinline));
void gen_auth(int id, char *src, unsigned len, char *dest)
{
	long long unonce = new_umac_nonce();
	memcpy(dest, (char*)&unonce, UNonce_size);
	dest += UNonce_size;

	for (int i=0; i < num_principals; i++)
	{
		// node never authenticates message to itself
		if (i == id)
		    continue;
		gen_mac(src, len, dest, id, i, (char*)&unonce);
		dest += UMAC_size;
	}
}

// returns next destination
char *gen_auth_single(int from, int to, char *src, unsigned len, char *dest)
{
	if (from == to)
		return dest;

	long long unonce = new_umac_nonce();
	memcpy(dest, (char*)&unonce, UNonce_size);
	dest += UNonce_size;

	// node never authenticates message to itself
	gen_mac(src, len, dest, from, to, (char*)&unonce);
	dest += UMAC_size;
#if 0
	fprintf(stderr, "Generated [%d]->[%d]: ", from, to);
	char *pos = dest - UMAC_size - UNonce_size;
	for (int k=0;k<UMAC_size+UNonce_size; k++)
		fprintf(stderr, "%02hhX ", pos[k]);
	fprintf(stderr, "\n");
#endif
	return dest;
}

bool verify_auth(int id, char *src, unsigned len, char *dest) __attribute__ ((noinline));
bool verify_auth(int id, char *src, unsigned len, char *dest)
{
	bool rv = true;
	for (int i=0; i < num_principals; i++) {
		if (i == id)
			continue;
		long long unonce;
		memcpy((char*)&unonce, dest, UNonce_size);

#if 0
		fprintf(stderr, "Verifying [%d]->[%d]: ", i, id);
		char *pos = dest;
		for (int k=0;k<UMAC_size+UNonce_size; k++)
			fprintf(stderr, "%02hhX ", pos[k]);
		fprintf(stderr, "\n");
#endif
		dest += UNonce_size;

		rv = rv && verify_mac(i, id, src, len, dest, (char*)&unonce);
		if (!rv) {
			fprintf(stderr, "Verification from %d failed\n", i);
			return false;
		}
		dest += UMAC_size;
	}
	return true;
}

pthread_mutex_t out_queue_mutex;
pthread_mutex_t in_queue_mutex;
pthread_cond_t not_empty_out_queue_cond;
pthread_cond_t not_empty_in_queue_cond;
char* the_out_queue;
char* the_in_queue;

size_t reqs = 0;
size_t num_iterations = 0;

void* sender_helper(void *) {
	char* msg = NULL;
	Digest dig;
	ticks send_ticks, recv_ticks;
	double total_time_digest = 0.0;
	double total_time_auth = 0.0;
	double total_time_verify = 0.0;
	double total_time_mem = 0.0;

	the_out_queue = NULL;

	int ccache_rv = 0; // will use this to prevent optimization

	for (size_t i=0; i<num_iterations; i++) {
	    // create request
	    msg = (char*)malloc(MSG_SIZE(reqs));
	    if (msg == NULL) {
	    	fprintf(stderr, "sender: can't allocate\n");
	    	exit(1);
	    }
	    // authenticate
		send_ticks = getticks();
	    comp_digest(msg, reqs, dig);
		recv_ticks = getticks();
		total_time_digest += (recv_ticks-send_ticks);
	   
		if (opt_macs) {
			send_ticks = getticks();
			gen_auth(0, msg, reqs, msg+reqs);
			recv_ticks = getticks();
			total_time_auth += (recv_ticks-send_ticks);
		}

	    // Enqueue the request
	    pthread_mutex_lock(&out_queue_mutex);
	    {
			the_out_queue = msg;
			pthread_cond_signal(&not_empty_out_queue_cond);
	    }
	    pthread_mutex_unlock(&out_queue_mutex);

		msg = NULL;
		pthread_mutex_lock(&in_queue_mutex);
		{
			while (the_in_queue == NULL)
			{
				pthread_cond_wait(&not_empty_in_queue_cond, &in_queue_mutex);
			}
			msg = the_in_queue;
			the_in_queue = NULL;
			pthread_cond_signal(&not_empty_in_queue_cond);
			//fprintf(stderr, "Got one\n");
		}
		pthread_mutex_unlock(&in_queue_mutex);

		// need to clean the cache
		size_t ccache_size = 32*1024;
		char *ccache_cleaner = (char*)malloc(ccache_size);
		for (size_t i=0; i<ccache_size; i=i+15) {
			ccache_cleaner[i] = rand()%256;
			if (i==150)
				ccache_rv += ccache_cleaner[i];
		}
		free(ccache_cleaner);
		
		send_ticks = getticks();
		char *nmsg = (char*)malloc(MAX_MSG_SIZE);
		void *nnptr = NULL;
		memcpy(nmsg, msg, MSG_SIZE(reqs));
		if (msg && nmsg && (nnptr = realloc(nmsg, MSG_SIZE(reqs)))) {
			free(msg);
			msg = (char*)nnptr;
		} else {
			if (msg) free(msg);
			continue;
		}
		recv_ticks = getticks();
		total_time_mem += (recv_ticks-send_ticks);

		// do the verification
		if (opt_macs) {
			send_ticks = getticks();
			bool rv = verify_auth(0, msg, reqs, msg+reqs);
			recv_ticks = getticks();
			total_time_verify += (recv_ticks-send_ticks);
			if (!rv) {
				fprintf(stderr, "Could not verify msg\n");
			}
		}

		free(msg);
	}
	fprintf(stdout, "%zu: %-16.3f %-16.3f %-16.3f %-16.3f\n", reqs,
					total_time_digest/num_iterations,
					total_time_auth/num_iterations,
					total_time_verify/num_iterations,
					total_time_mem/num_iterations);
	fprintf(stderr, "%d\n", ccache_rv);
	pthread_exit(NULL);
}

void* receiver_helper(void *) {
	char *nmsg = NULL;
	while (1) {
		pthread_mutex_lock(&out_queue_mutex);
		{
			while (the_out_queue == NULL)
			{
				pthread_cond_wait(&not_empty_out_queue_cond, &out_queue_mutex);
			}
			nmsg = the_out_queue;
			the_out_queue = NULL;
			pthread_cond_signal(&not_empty_out_queue_cond);
			//fprintf(stderr, "Got one\n");
		}
		pthread_mutex_unlock(&out_queue_mutex);

		char* msg = (char*)malloc(MAX_MSG_SIZE);
		memcpy(msg, nmsg, MSG_SIZE(reqs));

		free(nmsg);

	    if (opt_macs) {
	    	char *dst_pos = msg+reqs;
	    	for (int i=0; i<NUM_PRINCIPALS; i++) {
	    		dst_pos = gen_auth_single(i, 0, msg, reqs, dst_pos);
	    	}
	    }

	    pthread_mutex_lock(&in_queue_mutex);
	    {
			the_in_queue = msg;
			pthread_cond_signal(&not_empty_in_queue_cond);
	    }
	    pthread_mutex_unlock(&in_queue_mutex);

	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc != 3 && argc != 4) {
		fprintf(stderr, "%s: arguments are [--no_macs] <request size> <number of iterations>\n", argv[0]);
		exit(1);
	}
	int largc = 1;
	if (!strcmp(argv[largc], "--no_macs")) {
		opt_macs = false;
		largc++;
	}
	reqs = atoi(argv[largc++]);
	num_iterations = atoi(argv[largc++]);
	the_out_queue = NULL;
	the_in_queue = NULL;

	setup_umac();

	pthread_mutex_init(&out_queue_mutex, NULL);
	pthread_mutex_init(&in_queue_mutex, NULL);
	pthread_cond_init(&not_empty_out_queue_cond, NULL) ;
	pthread_cond_init(&not_empty_in_queue_cond, NULL) ;

	pthread_t sender;
	pthread_t receiver;

	if (pthread_create(&sender, NULL, &sender_helper, NULL) != 0) {
		fprintf(stderr, "Failed to create sender thread\n");
		exit(1);
	}

	if (pthread_create(&receiver, NULL, &receiver_helper, NULL) != 0) {
		fprintf(stderr, "Failed to create receiver thread\n");
		exit(1);
	}

	pthread_join(sender, NULL);
}
