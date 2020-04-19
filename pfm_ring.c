#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ring.h>
#include "pfm.h"
#include "pfm_ring.h"

void ring_write( struct rte_ring *ring_ptr,
                struct rte_mbuf *pkt_burst[],
                const int num_pkts)
{
	int pkts_send;

	pkts_send = rte_ring_enqueue_burst(ring_ptr,
			(void **)pkt_burst,num_pkts,NULL);
	if (pkts_send < num_pkts)
	{
                /* some packets are not send.
                   Drop them by releasing the buffers */
                pfm_log_rte_err(PFM_LOG_WARNING,
                        "Dropped %d slow path packets rte_kni_tx_burst()",
                        (num_pkts-pkts_send));

		for( ;  pkts_send < num_pkts; pkts_send++)
		{
			rte_pktmbuf_free(pkt_burst[pkts_send]);
		}
	}
	return;
}

