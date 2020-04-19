#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>

#include "pfm.h"
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
	int retVal;

	pfm_trace_msg("WORKER thread started");

	while(1)
	{
		if (PFM_FALSE != force_quit_g)
		{
			pfm_trace_msg("Stopping WORKER thread");
			return 0;
		}
		nbRx = rte_ring_dequeue_burst(	sys_info_g.rx_ring_ptr,
					(void **) rxPkts,
					RX_BURST_SIZE,
					NULL);
		if (0 >= nbRx) continue;


		nbTx = nbRx;
                for(uint16_t i = 0; i < nbTx; i++)
                {
			unsigned char *pkt;
			int pktLen;
			int j;

			pkt = rte_pktmbuf_mtod(rxPkts[i],unsigned char *);

			pktLen = rxPkts[i]->pkt_len;

			printf("PACKET %d [",i);
			for (j=0; j< pktLen; j++)
			{
				printf("%02X ",pkt[j]);
			}
			printf("]\n");

			/* Swap MAC Address */
			unsigned char x;
			for(j=0; j < 6; j++)
			{
				x = pkt[j];
				pkt[j] = pkt[j+6];
				pkt[j+6] = x;
			};
                        retVal = rte_ring_enqueue(      sys_info_g.tx_ring_ptr,
                                                        rxPkts[i]);
                        if (0 != retVal)
                        {
				pfm_log_rte_err(PFM_LOG_EMERG,
                                	"rte_ring_enqueue() failed");
                        }
                }
	}
	return 1;
}


