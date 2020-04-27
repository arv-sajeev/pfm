#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_tx_loop.h"
#include "pfm_link.h"
#include "pfm_kni.h"

int tx_loop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *tx_pkts[TX_BURST_SIZE];
	uint16_t rx_sz,tx_sz;
	int link_id;
	pfm_trace_msg("TX thread started");

	while(PFM_TRUE != force_quit_g)
	{
		rx_sz = kni_read(tx_pkts,TX_BURST_SIZE,&link_id);

		if (0 >= rx_sz)
		{
			rx_sz = rte_ring_dequeue_burst(
					sys_info_g.tx_ring_ptr,
					(void **)tx_pkts,
					TX_BURST_SIZE,
					NULL);
			if (rx_sz >0){
				pfm_trace_msg("%d packets from distLoop",
					      rx_sz);
				//printf("%d packets from distLoop\n",rx_sz);
				link_id = 0;
			}
		}

		else {
			pfm_trace_msg("%d packets from KNI",
				      rx_sz);

			//printf("%d packets from KNI\n",rx_sz);
		}

		if (0 >= rx_sz) continue;


		tx_sz = rte_eth_tx_burst(link_id,0,tx_pkts,rx_sz);
		pfm_trace_msg("%d packets sent to link :: %d",
			      tx_sz,
		              link_id);

		//printf("%d packets sent to link :: %d\n",tx_sz,link_id);

		if (tx_sz < rx_sz)
		{
			/* free buffers of the droped Pkts */
			pfm_trace_msg("Dropping packets from the txLoop");
			//printf("Dropping packets from the txLoop\n");
			for (; tx_sz < rx_sz; tx_sz++)
			{
				rte_pktmbuf_free(tx_pkts[tx_sz]);
			}
		}
	}

	pfm_trace_msg("Exiting tx thread on lcore [%d]",
		       rte_lcore_id());
	return 1;
}


