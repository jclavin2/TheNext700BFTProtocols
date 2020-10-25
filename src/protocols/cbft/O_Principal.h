#ifndef _Q_Principal_h
#define _Q_Principal_h 1

#include <string.h>
#include <sys/time.h>
#include "types.h"
#include "O_Cryptography.h"
#include "Traces.h"

//#define USE_SECRET_SUFFIX_MD5
extern "C"
{
#include "umac.h"
}
class O_Node;
class rabin_pub;

class O_Principal
{
	public:
		O_Principal(int i, int num_principals, Addr a, char *p);
		//Added by Maysam Yabandeh
		O_Principal(int i, int num_principals, Addr a, char* ip, int port, char *p);
		// Effects: Creates a new Principal object.

		virtual ~O_Principal();
		// Effects: Deallocates all the storage associated with principal.

		int pid() const;
		// Effects: Returns the principal identifier.

		const Addr *address() const;
		// Effects: Returns a pointer to the principal's address.

		//
		// Cryptography:
		//
		bool verify_mac(const char *src, unsigned src_len, const char *mac,
				const char *unonce);
		// Effects: Returns true iff "mac" is a valid MAC generated by
		// the key for dest_pid for "src_len" bytes starting at "src".

		bool verify_mac(const char *src, unsigned src_len, const char *mac);

		void gen_mac(const char *src, unsigned src_len, char *dst,
				int dest_pid, const char *unonce);
		// Requires: "dst" can hold at least "MAC_size" bytes.
		// Effects: Generates a MAC (with MAC_size bytes) using they key for dest_pid and
		// places it in "dst".  The MAC authenticates "src_len" bytes
		// starting at "src".

		void
				gen_mac(const char *src, unsigned src_len, char *dst,
						int dest_pid);

		inline static long long new_umac_nonce()
		{
			return ++umac_nonce;
		}

		unsigned int sig_size() const;
		// Effects: Returns the size of signatures generated by this principal.

		bool verify_signature(const char *src, unsigned src_len, const char *sig, 
			bool allow_self=false);
		// Requires: "sig" is at least sig_size() bytes.
		// Effects: Checks a signature "sig" (from this principal) for
		// "src_len" bytes starting at "src". If "allow_self" is false, it
		// always returns false if "this->id == PBFT_R_node->id()"; otherwise,
		// returns true if signature is valid.

		unsigned encrypt(const char *src, unsigned src_len, char *dst, unsigned dst_len);
		// Effects: Encrypts "src_len" bytes starting at "src" using this
		// principal's public-key and places up to "dst_len" of the result in "dst".
		// Returns the number of bytes placed in "dst".

//Added by Maysam Yabandeh
      int remotefd;
		int port;
		char ip[100];

	private:
		int id;
		Addr addr;
		rabin_pub *pkey;
		int ssize;                // signature size

		// UMAC contexts used to generate MACs for incoming and outgoing messages
		umac_ctx_t* ctxs;

		static long long umac_nonce;

};

inline const Addr *O_Principal::address() const
{
	return &addr;
}

inline int O_Principal::pid() const
{
	return id;
}

inline bool O_Principal::verify_mac(const char *src, unsigned src_len,
		const char *mac)
{
	return verify_mac(src, src_len, mac+O_UNonce_size, mac);
}

inline void O_Principal::gen_mac(const char *src, unsigned src_len, char *dst,
		int dest_pid)
{
	++umac_nonce;
	memcpy(dst, (char*)&umac_nonce, O_UNonce_size);
	dst += O_UNonce_size;
	gen_mac(src, src_len, dst, dest_pid, (char*)&umac_nonce);
}

inline unsigned int O_Principal::sig_size() const { return ssize; }

#endif // _Q_Principal_h
