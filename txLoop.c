#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "txLoop.h"
#include "link.h"
#include "kni.h"

int txLoop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *txPkts[TX_BURST_SIZE*2];
	uint16_t nbTx, nbTxOut;
	int linkId;
	pfm_trace_msg("TX thread started");

	while(1)
	{
		if (PFM_FALSE != force_quit_g)
		{
			pfm_trace_msg("Stopping TX thread");
			return 0;
		}
	
		nbTx = KniRead(txPkts,TX_BURST_SIZE,&linkId);
		if (0 >= nbTx)
		{
			nbTx = rte_ring_dequeue_burst(
					sys_info_g.tx_ring_ptr,
					(void **)txPkts,
					TX_BURST_SIZE,
					NULL);
			linkId=0;
		}
		if (0 >= nbTx) continue;


		nbTxOut = rte_eth_tx_burst(	linkId, 
						0, // only once queue
						txPkts,
						nbTx);
//printf("Tx %d->%d\n",nbTx,nbTxOut);

		if (nbTxOut < nbTx)
		{
			/* free buffers of the droped Pkts */
			for (; nbTxOut < nbTx; nbTxOut++)
			{
				rte_pktmbuf_free(txPkts[nbTxOut]);
			}
		}
	}
	return 1;
}


