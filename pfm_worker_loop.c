#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>


#include "pfm.h"
#include "pfm_log.h"
#include "pfm_comm.h"
#include "pfm_worker_loop.h"

/*************
 * pfm_data_ind()
 * 
 * When DPPF wants to pass a packet to applicaion, it invokes this function.
 * This is a dummy function which need to be implemented by application
 * It needs to be replaced when applicaiton is implemented.
 *
 ****************/
void pfm_data_ind(      const uint32_t local_ip,
                        const uint16_t port_num,
                        const uint16_t tunnel_id,
                        struct rte_mbuf *mbuf)
{
        printf("Invoked pfm_data_ind(local_ip=%d,port_num=%d,"
		"tunnel_id=%d,mbuf=%p\n). But not implemented\n",
                        local_ip,port_num, tunnel_id, mbuf);

	rte_pktmbuf_free(mbuf);
        return;
}


int worker_loop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *rx_pkts[8];
	struct rte_mbuf *tx_pkts[TX_BURST_SIZE];

	uint16_t rx_sz = 0;
	uint16_t tx_sz = 0;

	struct rte_distributor *dist = sys_info_g.dist_ptr;

	// Clear up all the buffers initailly 
	for (int i = 0;i < 8;i++)	{
		rx_pkts[i] = NULL;
	}

	for (int i = 0;i < TX_BURST_SIZE;i++)	{
		tx_pkts[i] = NULL;
	}
	
	pfm_trace_msg("Started worker on lcore [%d]",
		      rte_lcore_id());
	printf("Started worker on lcore [%d]\n",
		      rte_lcore_id());

	while (force_quit_g != PFM_TRUE)	{

		// Receive packets to be processed and send back packets that have completed processing 
		rx_sz = rte_distributor_get_pkt(dist,
						rte_lcore_id() - 4,  // The worker id is 4 - lcoreid
						rx_pkts,
						tx_pkts,
						tx_sz);
		if (tx_sz > 0 )	{

			pfm_trace_msg("Sent %d packets back to distributor from worker core [%d]",
				      tx_sz,
				      rte_lcore_id());
		
			printf("Sent %d packets back to distributor from worker core [%d]\n",
				      tx_sz,
				      rte_lcore_id());
		}


		if (rx_sz > 0 )	{

			pfm_trace_msg("Received %d packets from distributor on worker core [%d]",
				      rx_sz,
				      rte_lcore_id());

			printf("Received %d packets from distributor on worker core [%d]\n",
				      rx_sz,
				      rte_lcore_id());
		}

		// Do some processing
		tx_sz = rx_sz;

		for (uint16_t i = 0;i < tx_sz;i++)	{
			unsigned char *pkt = rte_pktmbuf_mtod(rx_pkts[i],unsigned char*);
		       	int pkt_len = rx_pkts[i]->pkt_len;
			pfm_trace_pkt_hdr(pkt,pkt_len,"Rx on worker [%d]",rte_lcore_id());
			pfm_trace_pkt(pkt,pkt_len,"Rx on worker [%d]",rte_lcore_id());
			printf("Processing packet #%d\n",i);

			//Swap macs
			
			unsigned char x;
			for (uint16_t j = 0;j < 6;j++)	{
				x = pkt[j];
				pkt[j] = pkt[j+6];
				pkt[j+6] = x;
			}	
			tx_pkts[i] = rx_pkts[i];
		}
	}
	pfm_log_msg(RTE_LOG_CRIT,
		    "Exiting worker thread on lcore [%d]",
		    rte_lcore_id());
	return 1;
}


