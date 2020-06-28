#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "pfm_cli.h"
#include "pfm_route.h"

#define GTP_PORTNO	2152

static void terminate_program(void)
{
	pfm_terminate();
	pfm_log_close();
	return;
}

static void signal_handler(int sig_num)
{
	if (    ( SIGINT == sig_num ) ||
		( SIGTERM == sig_num ))
	{
		pfm_trace_msg("Signal %d received, preparing to exit..",
					sig_num);
		terminate_program();
	}
	return;
}


int main(int argc, char *argv[])
{
	pfm_retval_t ret_val;
	pfm_cli_retval_t cli_ret_val;
	int ret;
	int lcore_id;

	/* initialize logging and tracing libs*/
	pfm_log_open("CUUP",PFM_LOG_DEBUG);

	/* install signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret_val = pfm_init(argc, argv);
	if (PFM_SUCCESS != ret_val)
	{
		terminate_program();
		exit(1);
	}

	pfm_link_open("0000:00:09.0");
	pfm_link_open("0000:00:0a.0");

	pfm_ip_addr_t ip;

	ip = pfm_str2ip("192.168.57.200");
	ret_val = pfm_ingress_classifier_add(0,"NGu0",ip,24,GTP_PORTNO);

	ip = pfm_str2ip("192.168.57.201");
	ret_val = pfm_ingress_classifier_add(0,"NGu1",ip,24,GTP_PORTNO);


	ip = pfm_str2ip("192.168.58.202");
	ret_val = pfm_ingress_classifier_add(1,"F1u0",ip,24,GTP_PORTNO);

	ip = pfm_str2ip("192.168.58.203");
	ret_val = pfm_ingress_classifier_add(1,"F1u1",ip,24,GTP_PORTNO);

	if (PFM_SUCCESS != ret_val)
	{
        	pfm_log_msg(PFM_LOG_WARNING,
			"KNI Port creation failed");
		exit(3);
	}

	ret_val = pfm_start_pkt_processing();
	if (PFM_SUCCESS != ret_val)
	{
        	pfm_log_msg(PFM_LOG_WARNING,
			"pfm_start_pkt_processing() failed");
		terminate_program();
		exit(2);
	}

	pfm_cli_init();

	cli_ret_val = PFM_CLI_CONTINUE;
	while((PFM_CLI_EXIT != cli_ret_val) &&
		(PFM_CLI_SHUTDOWN != cli_ret_val))
	{       
		cli_ret_val = pfm_cli_exec(stdin,stdout);
	};


	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		ret = rte_eal_wait_lcore(lcore_id);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_WARNING,
				"rte_eal_wait_lcore(lcore=%d) failed",
				lcore_id);
			exit(3);
		}
		else
		{
			pfm_trace_msg("lcore %d stopped", lcore_id);
		}
	}
	pfm_trace_msg("All slave threads stopped");

	terminate_program();
	exit(0);
}

/*************
 *
 * pfm_data_ind()
 * 
 * When PFM wants to pass a packet to application, it invokes this function.
 * This is a dummy function which need to be implemented by application
 * It needs to be replaced when application is implemented.
 *
 *
 ****************/

/*
void pfm_data_ind(      const pfm_ip_addr_t remote_ip_addr,
			const pfm_ip_addr_t local_ip_addr,
			pfm_protocol_t protocol,
			const int remote_port_no,
			const int local_port_no,
			struct rte_mbuf *mbuf)
{
	unsigned char *pkt;
	int pkt_len;

	char local_ip_addr_str[STR_IP_ADDR_SIZE];
	char remote_ip_addr_str[STR_IP_ADDR_SIZE];

        printf("Invoked pfm_data_ind(remote_ip=%s,local_ip=%s,"
			"protocol=%d,"
			"remote_port=%d,local_port=%d,buffer pointer=%p\n",
			pfm_ip2str(remote_ip_addr,remote_ip_addr_str),
			pfm_ip2str(local_ip_addr,local_ip_addr_str),
			protocol,
			remote_port_no,
			local_port_no,
			mbuf);

	pkt = rte_pktmbuf_mtod(mbuf,unsigned char*);
	pkt_len = mbuf->pkt_len;
	pfm_trace_pkt_hdr(pkt,pkt_len,"Rx on worker [%d]",rte_lcore_id());
	pfm_trace_pkt(pkt,pkt_len,"Rx on worker [%d]",rte_lcore_id());
	
	pfm_data_req(	local_ip_addr, remote_ip_addr, 
			protocol,
			local_port_no,remote_port_no,
			mbuf);

        return;
}


*/
