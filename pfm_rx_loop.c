#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_kni.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_rx_loop.h"
#include "pfm_link.h"
#include "pfm_classifier.h"

int rx_loop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *rx_pkts[RX_BURST_SIZE*2];
	uint16_t rx_sz;
	uint16_t link_id;

	pfm_trace_msg("rxLoop launced on lcore [%d]",
			rte_lcore_id());
	while(1)
	{
		if (PFM_FALSE != force_quit_g)
		{
			pfm_trace_msg("Stopping RX thread");
			return 0;
		}
		rx_sz = link_read(rx_pkts, RX_BURST_SIZE, &link_id);
		if (0 >= rx_sz) continue;
		
		ingress_classify(link_id,rx_pkts,rx_sz);
	}
	return 1;
}


