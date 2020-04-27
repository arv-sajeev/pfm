#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_link.h"
#include "pfm_rx_loop.h"
#include "pfm_tx_loop.h"
#include "pfm_dist_loop.h"
#include "pfm_worker_loop.h"
#include "pfm_classifier.h"
#include "pfm_kni.h"

volatile pfm_bool_t	force_quit_g =  PFM_FALSE;
sys_info_t	sys_info_g =
{
	.lcore_count	= 0,
	.kni_ptr	= NULL,
	.local_ip_count	= 0,
        .mbuf_pool	= NULL,
        .rx_ring_ptr	= NULL,
        .tx_ring_ptr	= NULL,
	.dist_ptr 	= NULL
};

pfm_retval_t pfm_init(int argc, char *argv[])
{
	int ret;
	int lcore_count;

	ret = rte_eal_init(argc, argv);
	if (0 > ret)
	{
		pfm_log_rte_err(PFM_LOG_EMERG,
	 		"rte_eal_init(argc=%d,argc=%p) failed. Terminating",
		 	argc, argv);
		return PFM_FAILED;
	}
	pfm_trace_msg("EAL Initialized");

	lcore_count = rte_lcore_count();
 	pfm_trace_msg("Detected %d lcores",lcore_count);

	if ( LCORE_MIN_LCORE_COUNT > lcore_count)
	{
	 	pfm_log_msg(PFM_LOG_EMERG,
		 	"Only %d lcore detected. %d are required. Terminating",
 			lcore_count,LCORE_MIN_LCORE_COUNT);
		return PFM_FAILED;
	}

	sys_info_g.lcore_count = lcore_count;

        sys_info_g.mbuf_pool = rte_pktmbuf_pool_create(
				MBUF_NAME,
				MBUF_COUNT,
				MBUF_CASHE_SIZE,
				MBUF_PRIV_SIZE,
				RTE_MBUF_DEFAULT_BUF_SIZE,
				SOCKET_ID_ANY);
        if (NULL == sys_info_g.mbuf_pool)
        {
		pfm_log_rte_err(PFM_LOG_EMERG,
			"rte_pktmbuf_pool_create() failed. Terminating.");
		return PFM_FAILED;
        }
        pfm_trace_msg("Mempool created");

        sys_info_g.tx_ring_ptr = rte_ring_create(
                                        TX_RING_NAME,
                                        TX_RING_SIZE,
                                        rte_socket_id(),
                                        RING_F_SC_DEQ
                                        );
        if (NULL == sys_info_g.tx_ring_ptr)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
			"rte_ring_create(ring=%s,ringSize=%d,socket=%d) failed."
			" Terminating."
			TX_RING_NAME,TX_RING_SIZE, rte_socket_id());
		return PFM_FAILED;
        }
	printf("Tx Ring Ptr=%p\n",sys_info_g.tx_ring_ptr);

        pfm_trace_msg("Ring '%s' opened",TX_RING_NAME);

        sys_info_g.rx_ring_ptr = rte_ring_create(
                                        RX_RING_NAME,
                                        RX_RING_SIZE,
                                        rte_socket_id(),
                                        RING_F_SP_ENQ
                                        );
        if (NULL == sys_info_g.rx_ring_ptr)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
			"rte_ring_create(ring=%s,ringSize=%d,socket=%d) failed."
			" Terminating."
			RX_RING_NAME,RX_RING_SIZE, rte_socket_id());
		return PFM_FAILED;
        }

        pfm_trace_msg("Ring '%s' opened",RX_RING_NAME);

	sys_info_g.dist_ptr = rte_distributor_create(DIST_NAME,
						    rte_socket_id(),
						    rte_lcore_count() - 4,
						    RTE_DIST_ALG_BURST);
	if (NULL == sys_info_g.dist_ptr)	{
		pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_distributor_create(distributor=%s,socket=%d,workers=%d,RTE_DIST_ALG_BURST"
				"Terminating",
				DIST_NAME,rte_socket_id(),rte_lcore_count()-4);
		return PFM_FAILED;
	}

 	pfm_trace_msg("DPPF Initialization successful");

	return PFM_SUCCESS;
}

pfm_retval_t pfm_start_pkt_processing(void)
{
	int ret;
	int worker_count=0;

        ret = rte_eal_remote_launch(tx_loop, NULL, LCORE_TXLOOP);
	if (0 != ret)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_eal_remote_launch(txLoop) failed");
		return PFM_FAILED;
        }
        pfm_trace_msg("TX thread started");
	
	ret = rte_eal_remote_launch(dist_loop,NULL,LCORE_DISTRIBUTOR);
	if (0 != ret)	{

		pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_eal_remote_launch(distLoop) failed");
		return PFM_FAILED;
	}
	pfm_trace_msg("Dist thread started ");

	for(int lc=0; lc < sys_info_g.lcore_count; lc++)
	{
		if (    (lc != LCORE_MAIN) &&
			(lc != LCORE_RXLOOP) &&
			(lc != LCORE_TXLOOP) &&
			(lc != LCORE_DISTRIBUTOR))
		{
			ret = rte_eal_remote_launch(worker_loop,NULL,lc);
			if (0 != ret)
			{
                		pfm_log_rte_err(PFM_LOG_ERR,
					"rte_eal_remote_launch"
					"(WORKER lcore=%d) failed",lc);
			} else
			{
				worker_count++;
				pfm_trace_msg("WORKER lcore %d started",lc);
			}
		}
	}

	if (worker_count > 0)
	{
		pfm_trace_msg("%d WORKERs lcore started",worker_count);
	}

	else
	{
               	pfm_log_msg(PFM_LOG_EMERG,
			"Failed to start any WORKER lcores. Terminating.");
		return PFM_FAILED;
	}

	ret = rte_eal_remote_launch(rx_loop, NULL, LCORE_RXLOOP);
        if (0 != ret)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_eal_remote_launch(rxLoop) failed");
		return PFM_FAILED;
        }
        pfm_trace_msg("RX thread started");
	return PFM_SUCCESS;
}

pfm_retval_t pfm_end_point_add(	const int link_id,
				const char *kni_name,
				const uint32_t local_ip_addr,
				const uint16_t local_gtp_port_num,
                        	const end_point_info_t ep[],
				const int ep_count)
{
	printf("link_id=%d, kni_name=%p, lIp=%d, port=%d, ep=%p, ep_count=%d\n",
		link_id,kni_name,local_ip_addr,local_gtp_port_num,ep,ep_count);
	return 1;
}

void pfm_data_req(	const uint32_t remote_ip,
			const uint16_t port_num,
			const uint16_t tunnel_id,
			struct rte_mbuf *mbuf)
{
	printf("remote_ip=%d,port_num=%d,tunnel_id=%d,mbuf=%p\n",
			remote_ip,port_num, tunnel_id, mbuf);
	return;
}
void pfm_terminate(void)
{
	force_quit_g = PFM_TRUE;
	return;
}

void link_state_change_callback(int link_id, ops_state_t ops_state)
{
	printf("Link state of link %d changed to %s\n",link_id,
                (( OPSSTATE_ENABLED == ops_state) ? "ENABLED": "DISABLED"));
	link_state_change(link_id,ops_state);
}


