#include <stdlib.h>
#include <strings.h>
#include "th_assert.h"
#include "O_Message_tags.h"
#include "O_Request.h"
#include "O_Node.h"
#include "O_Principal.h"
#include "MD5.h"

// extra & 1 = read only

O_Request::O_Request(O_Request_rep *r) :
    O_Message(r) {}

O_Request::O_Request(Request_id r, short rr) :
	O_Message(O_Request_tag, O_Max_message_size) {
	rep().cid = O_node->id();
	rep().rid = r;
	rep().replier = rr;
	rep().command_size = 0;
	rep().unused = 0;
	set_size(sizeof(O_Request_rep));
}

O_Request::~O_Request() {
}

char *O_Request::store_command(int &max_len) {
	max_len = msize() - sizeof(O_Request_rep) - O_node->auth_size();
	return contents() + sizeof(O_Request_rep);
}

inline void O_Request::comp_digest(Digest& d) {
	MD5_CTX context;
	MD5Init(&context);
	MD5Update(&context, (char*) &(rep().cid), sizeof(int) + sizeof(Request_id)
			+ rep().command_size);
	MD5Final(d.udigest(), &context);
}

void O_Request::authenticate(int act_len, bool read_only) {
	th_assert((unsigned)act_len <= msize() - sizeof(O_Request_rep)
			- O_node->auth_size(), "Invalid request size");

	// rep().extra = ((read_only) ? 1 : 0);
	rep().extra &= ~1;
	if (read_only) {
		rep().extra = rep().extra | 1;
	}

	rep().command_size = act_len;
	if (rep().replier == -1) {
		rep().replier = lrand48() % O_node->n();
	}
	//fprintf(stderr,"+++ %hd %hd %d %d %ld\n", rep().replier, rep().command_size, rep().cid, rep().rid, rep().unused);
	comp_digest(rep().od);

	int old_size = sizeof(O_Request_rep) + act_len;
	set_size(old_size + O_node->auth_size());
	O_node->gen_auth(contents(), sizeof(O_Request_rep), contents() + old_size);
}

bool O_Request::verify() {
	const int nid = O_node->id();
	const int cid = client_id();
	const int old_size = sizeof(O_Request_rep) + rep().command_size;
	O_Principal* p = O_node->i_to_p(cid);
	Digest d;

	comp_digest(d);
	if (p != 0 && d == rep().od) {
		// Verifying the authenticator.
		if (cid != nid && cid >= O_node->n() && size() - old_size
				>= O_node->auth_size(cid)) {
			return O_node->verify_auth(cid, contents(), sizeof(O_Request_rep),
					contents() + old_size);
		}
	}
	return false;
}

bool O_Request::convert(O_Message *m1, O_Request *&m2) {
	if (!m1->has_tag(O_Request_tag, sizeof(O_Request_rep))) {
		return false;
	}

	m2 = new O_Request(true);
	memcpy(m2->contents(), m1->contents(), m1->size());
	delete m1;
	m2->trim();
	return true;
}

inline O_Request_rep& O_Request::rep() const {
	th_assert(ALIGNED(msg), "Improperly aligned pointer");
	return *((O_Request_rep*) msg);
}

void O_Request::display() const {
	fprintf(stderr, "Request display: (client_id = %d)\n", rep().cid);
}
