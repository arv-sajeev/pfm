#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>


#include "pfm.h"
#include "pfm_log.h"
#include "pfm_comm.h"
#include "workerLoop.h"

/*************
 * pfm_data_ind()
 * 
 * When DPPF wants to pass a packet to applicaion, it invokes this function.
 * This is a dummy function which need to be implemented by application
 * It needs to be replaced when applicaiton is implemented.
 *
 ****************/
void pfm_data_ind(      const uint32_t localIp,
                        const uint16_t portNum,
                        const uint16_t tunnelId,
                        struct rte_mbuf *mbuf)
{
        printf("Invoked pfm_data_ind(localIp=%d,portNum=%d,"
		"tunnelId=%d,mbuf=%p\n). But not implemented\n",
                        localIp,portNum, tunnelId, mbuf);

	rte_pktmbuf_free(mbuf);
        return;
}


int workerLoop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *rxPkts[RX_BURST_SIZE*2];
	uint16_t nbRx;
	uint16_t nbTx;
	struct rte_distributor *dist = sys_info_g.dist_ptr;

	pfm_trace_msg("WORKER thread started");

	while(1)
	{
		if (PFM_FALSE != force_quit_g)
		{
			pfm_trace_msg("Stopping WORKER thread");
			return 0;
		}
		nbRx = rte_distributor_get_pkt(dist,
					       rte_lcore_id(),
					       rxPkts,
					       rxPkts,
					       RX_BURST_SIZE);
					
		if (0 >= nbRx) continue;

		pfm_trace_msg("Received %d packets on lcore %d",
			       nbRx,
			       rte_lcore_id()
			       );


		nbTx = nbRx;
                for(uint16_t i = 0; i < nbTx; i++)
                {
			unsigned char *pkt;
			int pktLen;
			int j;

			pkt = rte_pktmbuf_mtod(rxPkts[i],unsigned char *);

			pktLen = rxPkts[i]->pkt_len;
			pfm_trace_pkt_hdr(pkt,
					  pktLen,
					  "Received on worker at lcore [%d]",
					  rte_lcore_id());

			pfm_trace_pkt(pkt,
				      pktLen,
				      "Received on lcore [%d]",
				      rte_lcore_id());

			/* Swap MAC Address */
			unsigned char x;
			for(j=0; j < 6; j++)
			{
				x = pkt[j];
				pkt[j] = pkt[j+6];
				pkt[j+6] = x;
			};
			pfm_trace_pkt_hdr(pkt,
					  pktLen,
					  "swapped MAC addresses on worker at lcore [%d]",
					  rte_lcore_id());

                }
	}
	return 1;
}


