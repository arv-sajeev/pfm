#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "link.h"
#include "rxLoop.h"
#include "txLoop.h"
#include "distLoop.h"
#include "workerLoop.h"
#include "classifier.h"
#include "kni.h"

volatile pfm_bool_t	force_quit_g =  PFM_FALSE;
sys_info_t	sys_info_g =
{
	.lcore_count	= 0,
	.kni_ptr		= NULL,
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
	int workerCount=0;

        ret = rte_eal_remote_launch(txLoop, NULL, LCORE_TXLOOP);
	if (0 != ret)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_eal_remote_launch(txLoop) failed");
		return PFM_FAILED;
        }
        pfm_trace_msg("TX thread started");
	
	ret = rte_eal_remote_launch(distLoop,NULL,LCORE_DISTRIBUTOR);
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
			ret = rte_eal_remote_launch(workerLoop,NULL,lc);
			if (0 != ret)
			{
                		pfm_log_rte_err(PFM_LOG_ERR,
					"rte_eal_remote_launch"
					"(WORKER lcore=%d) failed",lc);
			} else
			{
				workerCount++;
				pfm_trace_msg("WORKER lcore %d started",lc);
			}
		}
	}

	if (workerCount > 0)
	{
		pfm_trace_msg("%d WORKERs lcore started",workerCount);
	}

	else
	{
               	pfm_log_msg(PFM_LOG_EMERG,
			"Failed to start any WORKER lcores. Terminating.");
		return PFM_FAILED;
	}

	ret = rte_eal_remote_launch(rxLoop, NULL, LCORE_RXLOOP);
        if (0 != ret)
        {
                pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_eal_remote_launch(rxLoop) failed");
		return PFM_FAILED;
        }
        pfm_trace_msg("RX thread started");
	return PFM_SUCCESS;
}

pfm_retval_t pfm_end_point_add(	const int linkId,
				const char *kpiName,
				const uint32_t localIpAddr,
				const uint16_t localGtpPortNum,
                        	const end_point_info_t ep[],
				const int epCount)
{
	printf("linkId=%d, kpiName=%p, lIp=%d, port=%d, ep=%p, epCount=%d\n",
		linkId,kpiName,localIpAddr,localGtpPortNum,ep,epCount);
	return 1;
}

void pfm_data_req(	const uint32_t remoteIp,
			const uint16_t portNum,
			const uint16_t tunnelId,
			struct rte_mbuf *mbuf)
{
	printf("remoteIp=%d,portNum=%d,tunnelId=%d,mbuf=%p\n",
			remoteIp,portNum, tunnelId, mbuf);
	return;
}
void pfm_terminate(void)
{
	force_quit_g = PFM_TRUE;
	return;
}

void LinkStateChangeCallback(int linkId, ops_state_t opsState)
{
	printf("Link state of link %d changed to %s\n",linkId,
                (( OPSSTATE_ENABLED == opsState) ? "ENABLED": "DISABLED"));
	KniStateChange(linkId,opsState);
}


