#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "pfm.h"
#include "pfm_comm.h"

#define GTP_PORTNO	2152

static void terminateProgram(void)
{
	pfm_terminate();
	pfm_log_close();
	return;
}

static void signalHandler(int sigNum)
{
	if (    ( SIGINT == sigNum ) ||
		( SIGTERM == sigNum ))
	{
		pfm_trace_msg("Signal %d received, preparing to exit..",
					sigNum);
		terminateProgram();
	}
	return;
}


int main(int argc, char *argv[])
{
	pfm_retval_t retVal;
	int ret;
	int lcoreId;

	/* initialize logging and tracing libs*/
	pfm_log_open("CUUP",PFM_LOG_DEBUG);

	/* install signal handlers */
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	retVal = pfm_init(argc, argv);
	if (PFM_SUCCESS != retVal)
	{
		terminateProgram();
		exit(1);
	}

	pfm_link_open("0000:00:09.0");
	pfm_link_open("0000:00:0a.0");

	unsigned char ip[5] = { 192,168,57,200 };
	retVal = pfm_ingress_classifier_add(0,"NGu0",ip,24,GTP_PORTNO);
	ip[3]++;
	retVal = pfm_ingress_classifier_add(0,"NGu1",ip,24,GTP_PORTNO);
	ip[3]++;
	ip[2]++;
	retVal = pfm_ingress_classifier_add(1,"F1u0",ip,24,GTP_PORTNO);
	ip[3]++;
	retVal = pfm_ingress_classifier_add(1,"F1u1",ip,24,GTP_PORTNO);
	ip[3]++;
	if (PFM_SUCCESS != retVal)
	{
        	pfm_log_msg(PFM_LOG_WARNING,
			"KNI Port creation failed");
		printf("KNI Port creation failed\n");
		exit(3);
	}

	retVal = pfm_start_pkt_processing();
	if (PFM_SUCCESS != retVal)
	{
        	pfm_log_msg(PFM_LOG_WARNING,
			"DPPF_StartPktProcessing() failed");
		terminateProgram();
		exit(2);
	}

	RTE_LCORE_FOREACH_SLAVE(lcoreId)
	{
		ret = rte_eal_wait_lcore(lcoreId);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_WARNING,
				"rte_eal_wait_lcore(lcore=%d) failed",
				lcoreId);
			exit(3);
		}
		else
		{
			pfm_trace_msg("lcore %d stopped", lcoreId);
		}
	}
	pfm_trace_msg("All slave threads stopped");

	terminateProgram();
	exit(0);
}


