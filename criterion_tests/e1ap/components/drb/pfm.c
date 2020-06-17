#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>

#include "pfm.h"
#include "pfm_comm.h"

static pfm_bool_t pfm_up = PFM_FALSE;
pfm_retval_t pfm_init(int argc, char *argv[])
{
	int ret;
	int lcore_count;
	if (pfm_up == PFM_FALSE)
	{
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
		pfm_trace_msg("DPPF Initialization successful");
		pfm_up = PFM_TRUE;
	}
	return PFM_SUCCESS;
}


