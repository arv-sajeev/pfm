#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_kni.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "rxLoop.h"
#include "link.h"
#include "classifier.h"

int rxLoop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *rxPkts[RX_BURST_SIZE*2];
	uint16_t nbRx;
	uint16_t linkId;

	pfm_trace_msg("RX thread started");

	while(1)
	{
		if (PFM_FALSE != force_quit_g)
		{
			pfm_trace_msg("Stopping RX thread");
			return 0;
		}
		nbRx = LinkRead(rxPkts, RX_BURST_SIZE, &linkId);
		if (0 >= nbRx) continue;
		
		IngressClassify(linkId,rxPkts,nbRx);
	}
	return 1;
}


