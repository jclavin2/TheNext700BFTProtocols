#ifndef _PBFT_R_Stable_estimatoPBFT_R_h
#define _PBFT_R_Stable_estimatoPBFT_R_h 1

#include "types.h"

class PBFT_R_Reply_stable;

class PBFT_R_Stable_estimator {
  //
  // Used to estimate the maximum stable checkpoint sequence number at
  // any non-faulty PBFT_R_replica by collecting reply-stable messages.
  //
public:
  PBFT_R_Stable_estimator();
  // Effects: Creates a stable information with no information.

  ~PBFT_R_Stable_estimator();
  // Effects: Deallocates all the storage associated with this.

  bool add(PBFT_R_Reply_stable* m, bool mine=false);
  // Effects: Adds message "m" to this and returns true if the
  // estimation is complete. "mine" should be true iff the message was
  // sent by the caller.

  Seqno estimate() const;
  // Effects: If the estimation is not complete, returns -1;
  // otherwise, returns the estimate of the maximum stable checkpoint
  // sequence number at any non-faulty PBFT_R_replica. This estimate is a
  // conservative upper bound.

  Seqno low_estimate();
  // Effects: Returns the maximum sequence number for a checkpoint
  // that is known to be stable. This estimate is a lower bound.

  void mark_stale();
  // Effects: If the estimation is complete, it has no effect,
  // Otherwise, discards all the information in this.

  void clear();
  // Effects: Discards all messages in this.

private:
  class Val {
  public:
    Seqno lc; // Minimum lc sent by corresponding PBFT_R_replica
    int lec;  // Number of values with lc less than or equal to this->lc 
    int gep;  // Number of values with lp greater than or equal to this->lc
   
    Seqno lp; // Maximum lp sent by corresponding PBFT_R_replica
     
    inline Val() { clear(); }
    inline void clear() { lc=Seqno_max; lec=0; lp=-1; gep=0; }
  };
  Val* vals; // vector with a value for each PBFT_R_replica indexed by PBFT_R_replica id.
  int nv;

  Seqno est; // estimate or -1 if not known
};

inline Seqno PBFT_R_Stable_estimator::estimate() const { return est; }

#endif // _PBFT_R_Stable_estimatoPBFT_R_h
