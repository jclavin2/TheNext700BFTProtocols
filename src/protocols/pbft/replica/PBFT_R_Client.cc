#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#include "th_assert.h"
#include "PBFT_R_Client.h"
#include "PBFT_R_ITimer.h"
#include "PBFT_R_Message.h"
#include "PBFT_R_Reply.h"
#include "PBFT_R_Request.h"

//#define ADJUST_RTIMEOUT 1

PBFT_R_Client::PBFT_R_Client(FILE *config_file, FILE *config_priv,
		char *host_name, short port) :
	PBFT_R_Node(config_file, config_priv, host_name, port), t_reps(2* f() + 1),
			c_reps(f() + 1)
{
	// Fail if node is is a PBFT_R_replica.
	if (is_PBFT_R_replica(id()))
		th_fail("PBFT_R_Node is a PBFT_R_replica");

	rtimeout = 150; // Initial timeout value
	rtimer = new PBFT_R_ITimer(rtimeout, PBFT_R_rtimePBFT_R_handler);

	out_rid = new_rid();
	out_req = 0;

	// Multicast new key to all PBFT_R_replicas.
	send_new_key();
	atimer->start();
}

PBFT_R_Client::~PBFT_R_Client()
{
	delete rtimer;
}

void PBFT_R_Client::reset()
{
	rtimeout = 150;
}

bool PBFT_R_Client::send_request(PBFT_R_Request *req)
{
	bool ro = req->is_read_only();
	if (out_req == 0)
	{
		// Send request to service
		if (ro || req->size() > PBFT_R_Request::big_req_thresh)
		{
			// read-only requests and big requests are multicast to all PBFT_R_replicas.
			//fprintf(stderr, "%d PBFT_R_Client:send_request to All_PBFT_R_replicas\n", PBFT_R_node->id());
			send(req, All_PBFT_R_replicas);
		} else
		{
			// read-write requests are sent to the primary only.
			//fprintf(stderr, "%d PBFT_R_Client:send_request to primary\n", PBFT_R_node->id());
			send(req, primary());
		}
		out_req = req;
		need_auth = false;
		n_retrans = 0;

#ifdef ADJUST_RTIMEOUT
		// Adjust timeout to reflect average latency
		rtimer->adjust(rtimeout);

		// Start timer to measure request latency
		latency.reset();
		latency.start();
#endif

		rtimer->start();
		return true;
	} else
	{
		// Another request is being processed.
		return false;
	}
}

PBFT_R_Reply *PBFT_R_Client::recv_reply()
{
	//fprintf(stderr, "%d recv reply is called\n", PBFT_R_node->id());
	if (out_req == 0)
		// Nothing to wait for.
		return 0;

	//
	// Wait for reply
	//
	while (1)
	{
		PBFT_R_Message* m = recv();
		//fprintf(stderr, "%d received one reply\n", PBFT_R_node->id());
		PBFT_R_Reply* rep;
		if (!PBFT_R_Reply::convert(m, rep) || rep->request_id() != out_rid)
		{
			delete m;
			continue;
		}

		PBFT_R_Certificate<PBFT_R_Reply> &reps = (rep->is_tentative()) ? t_reps
				: c_reps;
		if (reps.is_complete())
		{
			// We have a complete certificate without a full reply.
			if (!rep->full() || !rep->verify() || !rep->match(reps.cvalue()))
			{
				delete rep;
				continue;
			}
		} else
		{
		    reps.add(rep);

		    rep = (reps.is_complete() && reps.cvalue()->full()) ? reps.cvalue_clear() : 0;
		}
		if (rep)
		{
			// printf("request %d has committed\n", (int)rep->request_id());

			if (!rep->should_switch())
			    out_rid = new_rid();
			rtimer->stop();
			out_req = 0;
			t_reps.clear();
			c_reps.clear();

			// Choose view in returned rep. TODO: could make performance
			// more robust to attacks by picking the median view in the
			// certificate.
			v = rep->view();
			cuPBFT_R_primary = v % num_PBFT_R_replicas;

#ifdef ADJUST_RTIMEOUT
			latency.stop();
			rtimeout = (3*rtimeout+
					latency.elapsed()*Rtimeout_mult/(clock_mhz*1000))/4+1;
#endif

			return rep;
		}
	}
}

void PBFT_R_rtimePBFT_R_handler()
{
	th_assert(PBFT_R_node, "PBFT_R_Client is not initialized");
	((PBFT_R_Client*) PBFT_R_node)->retransmit();
}

void PBFT_R_Client::retransmit()
{
	// Retransmit any outstanding request.
	static const int thresh = 1;
	static const int nk_thresh = 4;
	static const int nk_thresh_1 = 100;

	if (out_req != 0)
	{
		INCPBFT_R_OP(req_retrans);

		//    fprintf(stderr, ".");
		n_retrans++;
		if (n_retrans == nk_thresh || n_retrans % nk_thresh_1 == 0)
		{
			send_new_key();
		}

		bool ro = out_req->is_read_only();
		bool change = (ro || out_req->replier() >= 0) && n_retrans > thresh;
		//    printf("%d %d %d %d\n", id(), n_retrans, ro, out_req->replier());

		if (need_auth || change)
		{
			// Compute new authenticator for request
			out_req->re_authenticate(change);
			need_auth = false;
			if (ro && change)
				t_reps.clear();
		}

		if (out_req->is_read_only() || n_retrans > thresh || out_req->size()
				> PBFT_R_Request::big_req_thresh)
		{
			// read-only requests, requests retransmitted more than
			// mcast_threshold times, and big requests are multicast to all
			// PBFT_R_replicas.
			//fprintf(stderr, "%d Retransmit\n", PBFT_R_node->id());
			send(out_req, All_PBFT_R_replicas);
		} else
		{
			// read-write requests are sent to the primary only.
			send(out_req, primary());
		}
	}

#ifdef ADJUST_RTIMEOUT
	// exponential back off
	if (rtimeout < Min_rtimeout) rtimeout = 100;
	rtimeout = rtimeout+lrand48()%rtimeout;
	if (rtimeout> Max_rtimeout) rtimeout = Max_rtimeout;
	rtimer->adjust(rtimeout);
#endif

	rtimer->restart();
}

void PBFT_R_Client::send_new_key()
{
	PBFT_R_Node::send_new_key();
	need_auth = true;

	// Cleanup reply messages authenticated with old keys.
	t_reps.clear();
	c_reps.clear();
}