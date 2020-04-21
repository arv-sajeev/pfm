#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_distributor.h>
#include <rte_ring.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "link.h"
#include "pfm_ring.h"
#include "distLoop.h"


int distLoop(__attribute__((unused)) void * args)	{
	struct rte_ring *rx_dist_ring = sys_info_g.rx_ring_ptr;
	struct rte_ring *dist_tx_ring = sys_info_g.tx_ring_ptr;
	struct rte_distributor *dist = sys_info_g.dist_ptr;
	struct rte_mbuf *pkt_burst[RX_BURST_SIZE];
	pfm_log_msg(PFM_LOG_DEBUG,
		    "Started distributor on lcore [%d]",
		    rte_lcore_id());

	while (PFM_TRUE != force_quit_g)	{
		int rx_sz = rte_ring_dequeue_burst(rx_dist_ring,
						(void *) pkt_burst,
						RX_BURST_SIZE,
						NULL);
		pfm_trace_msg("Received %d packets on distributor at lcore [%d]",
			       rx_sz,
			       rte_lcore_id());
		rte_distributor_process(dist,
					pkt_burst,
					rx_sz);

		rx_sz = rte_distributor_returned_pkts(dist,
						      pkt_burst,
						      TX_BURST_SIZE);
		pfm_trace_msg("Received %d packets from workers on distributor at lcore [%d]",
			       rx_sz,
			       rte_lcore_id());

		ring_write(dist_tx_ring,
			   pkt_burst,
			   TX_BURST_SIZE);

		pfm_trace_msg("Transmitted %d packets on distributor at lcore [%d]",
			       rx_sz,
			       rte_lcore_id());


	}


	pfm_trace_msg("exiting from distributor");
	rte_distributor_flush(dist);
	rte_distributor_clear_returns(dist);
	return 0;
}
