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
	
	struct rte_mbuf *pkt_burst[RX_BURST_SIZE],*ret_pkts[TX_BURST_SIZE];
	
	int tx_sz = 0,rx_sz = 0,ret_rx = 0,ret_tx = 0;
	
	pfm_log_msg(PFM_LOG_DEBUG,
		    "Started distributor on lcore [%d]",
		    rte_lcore_id());

	printf("Started distributor on lcore [%d]",
		rte_lcore_id());

	while (force_quit_g != PFM_TRUE)	{
		// Get packets from the ring

		rx_sz = rte_ring_dequeue_burst(rx_dist_ring,
					       (void *)pkt_burst,
					       RX_BURST_SIZE,
					       NULL);
		if (rx_sz > 0)	{
			pfm_trace_msg("Received %d packets from rx_loop on distLoop",
			      	      rx_sz);

			printf("Received %d packets from rx_loop on distLoop\n",
				rx_sz);
		}
		// Process these packets using distributor api 
		
		tx_sz = rte_distributor_process(dist,
						pkt_burst,
						rx_sz);
		if (tx_sz > 0)	{
			pfm_trace_msg("Transmitted %d packets from distLoop to various workerLoops",
			      	       tx_sz);
			printf("Transmitted %d packets from distLoop to various workerLoops\n",
			      	tx_sz);

		}
		
		// Get the processed packets from workers
		
		ret_rx = rte_distributor_returned_pkts(dist,
						       ret_pkts,
						       TX_BURST_SIZE);
		if (ret_rx > 0)	{
			pfm_trace_msg("Received %d packets from workers on distLoop",
				      ret_rx);
			printf("Received %d packets from workers on distLoop\n",
				ret_rx);
		}
		// Send the received packets to the txLoop
		
		ret_tx = rte_ring_enqueue_burst(dist_tx_ring,
						(void *)ret_pkts,
						ret_rx,
						NULL);
		if (ret_tx > 0)	{
			pfm_trace_msg("Enqueued %d packets from distLoop to txLoop",
				      ret_tx);
			printf("Enqueued %d packets from distLoop to txLoop\n",
			      	ret_tx);
		}
	}

	pfm_log_msg(PFM_LOG_ALERT,"Forced exit from distributor");
	printf("Exiting distributor");
	return 1;
}




