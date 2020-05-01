#include <stdio.h>                                               
#include <stdlib.h>                                                                                 
#include <signal.h>                                                                                  

#include "pfm.h"                                                                                     
#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm_link.h"
#inlcude "pfm_arp.h"

static void terminate_program(void)	{
	pfm_terminate();
	pfm_log_close();
}

static void signal_handler(int sig_num)	{
	if ((SIGINT == sig_num) || (SIGTERM == sig_num))	{
		pfm_trace_msg("Signal %d received, preparing to exit..",
			      sig_num);
		terminate_program();
	}
	return;
}


int main(int argc,char* argv[])	{
	pfm_retval_t ret_val;
	int ret;
	int lcore_id;

	pfm_log_open("ARP-UNIT-TEST",PFM_LOG_DEBUG);	
	signal(SIGINT,signal_handler);
	signal(SIGTERM,signal_handler);

	ret_val = pfm_init(argc,argv);
	if (PFM_SUCCESS != ret_val)	{
		terminate_program();
		exit(1);
	}

	pfm_link_open("0000:00;09.0");
	pfm_trace_msg("Init all done");

	while (1)	{
		ret = link_read(

	}




}



