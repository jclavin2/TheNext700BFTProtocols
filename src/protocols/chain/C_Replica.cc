#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "th_assert.h"
#include "C_Message_tags.h"
#include "C_Request.h"
#include "C_Checkpoint.h"
#include "C_Reply.h"
#include "C_Principal.h"
#include "C_Replica.h"
#include "MD5.h"

#define _MEASUREMENTS_ID_ (C_replica->id())
#include "measurements.h"
#include "statistics.h"

//#define TRACES

// Global replica object.
C_Replica *C_replica;

#include <signal.h>
static void kill_replica(int sig)
{
    REPORT_STATS;
    REPORT_TIMINGS;
    exit(0);
}

// Function for the thread receiving messages from the predecessor in the chain
//extern "C"
void*requests_from_predecessor_handler_helper(void *o)
{
	void **o2 = (void **)o;
	C_Replica &r = (C_Replica&)(*o2);
	r.requests_from_predecessor_handler();
	return 0;
}

// Function for the thread receiving messages from clients
//extern "C"
void*C_client_requests_handler_helper(void *o)
{
	void **o2 = (void **)o;
	C_Replica &r = (C_Replica&)(*o2);
	r.c_client_requests_handler();
	return 0;
}

inline void*C_message_queue_handler_helper(void *o)
{
	void **o2 = (void **)o;
	C_Replica &r = (C_Replica&) (*o2);
	//temp_replica_class = (Replica<class Request_T, class Reply_T>&)(*o);
	//  r.recv1();
	//temp_replica_class.do_recv_from_queue();
	r.do_recv_from_queue();
	return 0;
}

C_Replica::C_Replica(FILE *config_file, FILE *config_priv, char* host_name, short port) :
	C_Node(config_file, config_priv, host_name, port), seqno(0), last_seqno(0), replies(num_principals), checkpoint_store()
{
	// Fail if node is not a replica.
	if (!is_replica(id()))
	{
		th_fail("Node is not a replica");
	}

	seqno = 0;

	// Read view change, status, and recovery timeouts from replica's portion
	// of "config_file"
	int vt, st, rt;
	fscanf(config_file, "%d\n", &vt);
	fscanf(config_file, "%d\n", &st);
	fscanf(config_file, "%d\n", &rt);

	// Create timers and randomize times to avoid collisions.
	srand48(getpid());

	exec_command = 0;

#if 1
	struct sigaction act;
        act.sa_handler = kill_replica;
        sigemptyset (&act.sa_mask);
        act.sa_flags = 0;
        sigaction (SIGINT, &act, NULL);
        sigaction (SIGTERM, &act, NULL);
#endif

	// Create server socket
	in_socket= createServerSocket(ntohs(principals[node_id]->TCP_addr.sin_port));

	// Create receiving thread
	if (pthread_create(&requests_from_predecessor_handler_thread, 0,
			&requests_from_predecessor_handler_helper, (void *)this) != 0)
	{
		fprintf(stderr, "Failed to create the thread for receiving messages from predecessor in the chain\n");
		exit(1);
	}

	// Connect to principals[(node_id + 1) % num_r]
	out_socket = createClientSocket(principals[(node_id + 1) % num_replicas]->TCP_addr);

	if (node_id == 0 || node_id == (num_replicas - 1))
	{
		fprintf(stderr,"Creating client socket\n");
		in_socket_for_clients= createNonBlockingServerSocket(ntohs(principals[node_id]->TCP_addr_for_clients.sin_port));
		if (pthread_create(&C_client_requests_handler_thread, NULL,
				&C_client_requests_handler_helper, (void *)this)!= 0)
		{
			fprintf(stderr, "Failed to create the thread for receiving client requests\n");
			exit(1);
		}
	}
}

C_Replica::~C_Replica()
{
}

void C_Replica::do_recv_from_queue()
{
	C_Message *m;
	while (1)
	{
		pthread_mutex_lock(&incoming_queue_mutex);
		{
			while (incoming_queue.size() == 0)
			{

				pthread_cond_wait(&not_empty_incoming_queue_cond,
						&incoming_queue_mutex);
			}
			m = incoming_queue.remove();
		}
		pthread_mutex_unlock(&incoming_queue_mutex);
		handle(m);
	}
}

void C_Replica::register_exec(int(*e)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool))
{
	exec_command = e;
}

void C_Replica::requests_from_predecessor_handler()
{
	int socket = -1;

	listen(in_socket, 1);
	socket = accept(in_socket, NULL, NULL);
	if (socket < 0)
	{
		perror("Cannot accept connection \n");
		exit(-1);
	}

	// Loop to receive messages.
	while (1)
	{

		C_Message* m = C_Node::recv(socket);
		ENTRY_TIME_POS(1);
#ifdef TRACES
		fprintf(stderr, "Received message\n");
#endif
#ifdef DO_STATISTICS
		if (id() == 0) {
			UPDATE_IN(C,POST,m->size());
		} else {
			UPDATE_IN(R,POST,m->size());
		}
#endif
/* absolutely unnecessary
		C_Request *n;
		C_Checkpoint *ch;
		if (m->tag() == C_Request_tag && C_Request::convert(m, n))
		{
#ifdef TRACES
			fprintf(stderr, "Converted message to request\n");
#endif
*/
			// Enqueue the message
			pthread_mutex_lock(&incoming_queue_mutex);
			{
				incoming_queue.append(m);
				pthread_cond_signal(&not_empty_incoming_queue_cond);
			}
			pthread_mutex_unlock(&incoming_queue_mutex);
		EXIT_TIME_POS(1);
/* ditto
		} else if (m->tag() == C_Checkpoint_tag && C_Checkpoint::convert(m, ch))
		{
#ifdef TRACES
			fprintf(stderr, "Converted message to checkpoint\n");
#endif
		} else
		{
			delete m;
		}
*/
	}
}

void C_Replica::handle(C_Message *m)
{
	// TODO: This should probably be a jump table.
	switch (m->tag())
	{
		case C_Request_tag:
			ENTRY_TIME;
			gen_handle<C_Request>(m);
			EXIT_TIME;
			break;

		case C_Checkpoint_tag:
			gen_handle<C_Checkpoint>(m);
			break;

default:
			// Unknown message type.
			delete m;
	}
}

void C_Replica::handle(C_Request *req)
{
	int cid = req->client_id();

#ifdef TRACES
	fprintf(stderr, "*********** C_Replica %d: Receiving request to handle\n", id());
#endif

	if (unlikely(req->is_read_only()))
	{
		fprintf(stderr, "C_Replica %d: Read-only requests are currently not handled\n", id());
		delete req;
		return;
	}

	int authentication_offset=0;
	if (unlikely(!req->verify(&authentication_offset)))
	{
		fprintf(stderr, "C_Replica %d: verify returned FALSE\n", id());
		delete req;
		return;
	}
#ifdef TRACES
	fprintf(stderr, "*********** C_Replica %d: message verified\n", id());
#endif

	// different processing paths for the head and the rest of the nodes
	if (node_id == 0) {
		if (!is_seen(req)) {
			// new request, should execute, and reply to the client. afterwards, send order request to other replicas
			seqno++;
			execute_request(req);
			req->set_seqno(seqno);
		} else {
			if (!is_old(req)) {
				// Request has already been executed, and it was the last response, thus just reply back
                Seqno seq = replies.sent_seqno(req->client_id());
                req->set_seqno(seq);
			} else {
				// just discard...
				delete req;
				return;
			}
		}
	} else {
		if (!is_seen(req)) {
			// new request, should execute, and reply to the client. afterwards, send order request to other replicas
			if (req->seqno() != last_seqno+1) {
				fprintf(stderr, "C_Replica[%d]: out of order message (got: %lld), (expected: %lld)\n", id(), req->seqno(), last_seqno+1);
				// XXX: maybe panic?
				delete req;
				return;
			}
			// Execute req
			seqno = last_seqno+1;
			execute_request(req);
		} else {
			if (!is_old(req)) {
				// Request has already been executed, and it was the last response, thus just reply back
			}
		}
	}

	C_Reply_rep *rr = replies.reply_rep(req->client_id());
	req->authenticate(C_node->id(), authentication_offset, rr);

	if (node_id < num_replicas - 1)
	{
		// Forwarding the request
#ifdef TRACES
		fprintf(stderr, "C_Replica %d: Forwarding the request\n", id());
#endif
		int len = req->size();
		EXIT_TIME;
		send_all(out_socket, req->contents(), &len);
		UPDATE_OUT(R,POST,len);
		//return;
	}
	else
	{
		// C_Replying to the client
#ifdef TRACES
		fprintf(stderr, "C_Replying to the client\n");
#endif

		EXIT_TIME;
#ifdef DO_STATISTICS
	int sent =
#endif
		replies.send_reply(cid, connectlist[req->listnum()], req->MACs());
		UPDATE_OUT(C,POST,sent);
		//return;
	}

	delete req;
	EXIT_TIME;
	return;

	// send the checkpoint if necessary
	if (rh.should_checkpoint()) {
	    C_Checkpoint chkp;
	    // fill in the checkpoint message
	    chkp.set_seqno(rh.get_top_seqno());
	    chkp.set_digest(rh.rh_digest());
	    // sign
	    C_node->gen_signature(chkp.contents(), sizeof(C_Checkpoint_rep),
		    chkp.contents()+sizeof(C_Checkpoint_rep));
	    // send it
	    //send(chkp, C_All_replicas);
	}

}

void C_Replica::execute_request(C_Request *req)
{
	int cid = req->client_id();
	Request_id rid = req->request_id();

#ifdef TRACES
	fprintf(stderr, "C_Replica[%d]: executing request %lld for client %d\n", id(), rid, cid);
#endif

	// Obtain "in" and "out" buffers to call exec_command
	Byz_req inb;
	Byz_rep outb;
	Byz_buffer non_det;
	inb.contents = req->command(inb.size);
	outb.contents = replies.new_reply(cid, outb.size, seqno);
	//non_det.contents = pp->choices(non_det.size);

	// Execute command in a regular request.
	//C_BEGIN_TIME(exec);
	exec_command(&inb, &outb, &non_det, cid, false);
	//C_END_TIME(exec);

	// perform_checkpoint();

#if 0
	if (outb.size % ALIGNMENT_BYTES)
	{
		for (int i=0; i < ALIGNMENT_BYTES - (outb.size % ALIGNMENT_BYTES); i++)
		{
			outb.contents[outb.size+i] = 0;
		}
	}
#endif

	last_seqno = seqno;
	//Digest d;
	//rh->add_request(req, seqno, d);

	// Finish constructing the reply.
	replies.end_reply(cid, rid, outb.size);

	// send the checkpoint if necessary
#if 0
	if (0 && rh->should_checkpoint()) {
	    C_Checkpoint *chkp = new C_Checkpoint();
	    // fill in the checkpoint message
	    chkp->set_seqno(rh->get_top_seqno());
	    chkp->set_digest(rh->rh_digest());
	    // sign
	    C_node->gen_signature(chkp->contents(), sizeof(C_Checkpoint_rep),
		    	chkp->contents()+sizeof(C_Checkpoint_rep));
	    // add it to the store
	    C_CheckpointSet *checkpoints = NULL;
	    if (!checkpoint_store.find(chkp->get_seqno(), &checkpoints)) {
			checkpoints = new C_CheckpointSet(n());
			checkpoints->store(chkp);
			checkpoint_store.add(chkp->get_seqno(), checkpoints);
			//fprintf(stderr, "C_Replica[%d]: checkpoint seqno %lld added to the list\n", id(), chkp->get_seqno());
	    } else {
			//fprintf(stderr, "C_Replica[%d]: checkpoint set already exists for seqno %lld\n", id(), chkp->get_seqno());
	    }
	    // send it
	    send(chkp, C_All_replicas);
	}
#endif
}

// Returns true if request is old, i.e., there was newer request from the same client seen
bool C_Replica::is_old(C_Request *req)
{
	int cid = req->client_id();
	Request_id rid = req->request_id();

	if (rid < replies.req_id(cid))
	{
		return true;
	}

	return false;
}

// Returns true is request was seen
bool C_Replica::is_seen(C_Request *req)
{
	int cid = req->client_id();
	Request_id rid = req->request_id();

	if (rid <= replies.req_id(cid))
	{
		return true;
	}

	return false;
}

void C_Replica::handle(C_Checkpoint *c)
{
    // verify signature
    if (!c->verify()) {
	fprintf(stderr, "Couldn't verify the signature of C_Checkpoint\n");
	delete c;
	return;
    }

    // optimization: if less than last removed, discard
    if (checkpoint_store.last() != 0 && c->get_seqno() < checkpoint_store.last())
    {
	fprintf(stderr, "Checkpoint is for older than last removed, discarding\n");
	delete c;
	return;
    }

    // store
    CheckpointSet *checkpoints = NULL;
    if (!checkpoint_store.find(c->get_seqno(), checkpoints)) {
	checkpoints = new CheckpointSet(n());
	checkpoints->store(c);
	checkpoint_store.add(c->get_seqno(), checkpoints);
    } else {
	checkpoints->store(c);
    }
    // check whether full
    // if so, clear, and truncate history
    if (checkpoints->size() == n()) {
	fprintf(stderr, "Collected enough checkpoints\n");
	bool same = false;
	for (int i=0; i<n(); i++) {
	    C_Checkpoint *cc = checkpoints->fetch(i);
	    same = c->match(cc);
	    if (!same)
		break;
	}
	// we should now truncate the history
	if (same) {
	    fprintf(stderr, "All checkpoints are the same\n");
	    checkpoint_store.remove(c->get_seqno());
	    rh.truncate_history(c->get_seqno());
	}
    }
}

void C_Replica::c_client_requests_handler()
{
	struct timeval timeout;
	int readsocks;
	fd_set socks;

	fprintf(stderr,"Iam here\n");
	listen(in_socket_for_clients, MAX_CONNECTIONS);

	int highsock = in_socket_for_clients;
	memset((char *) &connectlist, 0, sizeof(connectlist));

	while (1)
	{
		FD_ZERO(&socks);
		FD_SET(in_socket_for_clients, &socks);

		// Loop through all the possible connections and add
		// those sockets to the fd_set
		for (int listnum = 0; listnum < MAX_CONNECTIONS; listnum++)
		{
			if (connectlist[listnum] != 0)
			{
				FD_SET(connectlist[listnum], &socks);
				if (connectlist[listnum] > highsock)
				{
					highsock = connectlist[listnum];
				}
			}
		}

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		readsocks = select(highsock + 1, &socks, (fd_set *) 0, (fd_set *) 0,
				&timeout);
		if (readsocks < 0)
		{
			perror("select");
			exit(1);
		}
		if (readsocks == 0)
		{
			//fprintf(stderr, ".");
			fflush(stdout);;
		} else
		{
			if (FD_ISSET(in_socket_for_clients, &socks))
			{
				handle_new_connection();
			}

			// Run through the sockets and check to see if anything
			// happened with them, if so 'service' them
			for (int listnum = 0; listnum < MAX_CONNECTIONS; listnum++)
			{
				// fprintf(stderr, "%d of %d, ",listnum,MAX_CONNECTIONS);
				if (FD_ISSET(connectlist[listnum], &socks))
				{
					C_Message* m = C_Node::recv(connectlist[listnum]);

					m->listnum() = listnum;
					// Enqueue the request
					pthread_mutex_lock(&incoming_queue_mutex);
					{
					    // fprintf(stderr, "Got the mutex\n");
					    incoming_queue.append(m);
					    UPDATE_IN(C,POST,m->size());
					    pthread_cond_signal(&not_empty_incoming_queue_cond);
					}
					pthread_mutex_unlock(&incoming_queue_mutex);
				}
			}
		}
	}
	pthread_exit(NULL);
}

void C_Replica::handle_new_connection()
{
	int listnum;
	int connection;

	// There is a new connection coming in
	// We  try to find a spot for it in connectlist
	connection = accept(in_socket_for_clients, NULL, NULL);
	if (connection < 0)
	{
		perror("accept");
		exit(EXIT_FAILURE);
	}
	setnonblocking(connection);
	for (listnum = 0; (listnum < MAX_CONNECTIONS) && (connection != -1); listnum ++)
	{
		if (connectlist[listnum] == 0)
		{
			fprintf(stderr, "\nConnection accepted:   FD=%d; Slot=%d\n", connection,
			listnum);
			connectlist[listnum] = connection;
			return;
		}
	}
	fprintf(stderr, "\nNo room left for new client.\n");
	close(connection);
}
