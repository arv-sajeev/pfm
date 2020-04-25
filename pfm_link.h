#ifndef __LINK_H__
#define __LINK_H__ 1
#include "pfm.h"
#include "pfm_comm.h"

int LinkRead(struct rte_mbuf *rxPkts[],uint16_t burstSize,uint16_t *port);

/* Application needs to implement the following callback functions */
void LinkStateChangeCallback(int portId, ops_state_t opsState);
void LinkStateChange(const int linkId,const ops_state_t desiredState);

#endif
