#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_tx_loop.h"
#include "pfm_link.h"
#include "pfm_kni.h"

static void tx_burst(	const int link_id,
			struct rte_mbuf **pkts,
			const int pkt_count)
{
	int tx_pkts;
	tx_pkts = rte_eth_tx_burst(link_id,0,pkts,pkt_count);
	pfm_trace_msg("Send %d out of %d pkts to LinkId=%d",
		tx_pkts,pkt_count,link_id);
	if (tx_pkts < pkt_count)
	{
		/* free buffers of the droped Pkts */
		pfm_trace_msg("Dropped %d pkts to LinkId=%d",
				(pkt_count - tx_pkts), link_id);
		for (; tx_pkts < pkt_count; tx_pkts++)
		{
			rte_pktmbuf_free(pkts[tx_pkts]);
		}
	}

	return;
}

int tx_loop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *tx_pkts[TX_BURST_SIZE];
	struct rte_mbuf *out_pkts[TX_BURST_SIZE];
	uint16_t in_count;
	int out_count;
	int next_pkt_idx;
	int link_id;
	int last_link_id = -1;
	pfm_trace_msg("TX thread started");

	while(PFM_TRUE != force_quit_g)
	{
		in_count = kni_read(tx_pkts,TX_BURST_SIZE,&link_id);
		if ( 0 < in_count)
		{
			pfm_trace_msg("%d pkts recvd from KNI\n",
				in_count);
			tx_burst(link_id,tx_pkts,in_count);
		}

		in_count = rte_ring_dequeue_burst(
				sys_info_g.tx_ring_ptr,
				(void **)tx_pkts,
				TX_BURST_SIZE,
				NULL);
		if (0 < in_count )
		{
			pfm_trace_msg("%d pkts recvd from distLoop\n",
				in_count);
			last_link_id = tx_pkts[0]->port;
			out_count = 0;
			for(next_pkt_idx=0;
				next_pkt_idx < in_count; next_pkt_idx++)
			{
				link_id = tx_pkts[next_pkt_idx]->port;
				if ( link_id != last_link_id)
				{
					/* send pkts for one link in a
					   one bust */
					tx_burst(last_link_id,
						out_pkts,out_count);
					out_count=0;
					last_link_id = link_id;
				}
				out_pkts[out_count] = tx_pkts[next_pkt_idx];
				out_count++;
			}
			if (0 < out_count)
			{
				/* Send last burst */
				tx_burst(last_link_id,
					out_pkts,out_count);
			}
		}

	}

	pfm_trace_msg("Exiting tx thread on lcore [%d]",
		       rte_lcore_id());
	return 1;
}


