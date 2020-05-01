#include <stdio.h>                                               
#include <stdlib.h>                                                                                 
#include <signal.h>       
#include <rte_common.h>

#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm_route.h"



int main(int argc,char* argv[])	{
	pfm_retval_t ret_val;
	int ret;
	int lcore_id;

	pfm_log_open("ARP-UNIT-TEST",PFM_LOG_DEBUG);	
	ret = rte_eal_init(argc,argv);
	lpm_init();
	// Test 1
	char ip_str[] 	= {23,1,168,192};
	char gate_way[] = {123,2,234,255};

	pfm_route_add(*(uint32_t*)ip_str,
		      20,
		      *(uint32_t*)gate_way,
		      1);
	pfm_route_print(stdout);
	
	pfm_route_add(*(uint32_t*)ip_str,
		      20,
		      *(uint32_t*)gate_way,
		      5);
	pfm_route_print(stdout);
	
	pfm_route_add(*(uint32_t*)ip_str,
		      23,
		      *(uint32_t*)gate_way,
		      3);
	pfm_route_print(stdout);

	ip_str[0] = 255;
	gate_way[1] = 0;

	pfm_route_add(*(uint32_t*)ip_str,
		      12,
		      *(uint32_t*)gate_way,
		      3);
	pfm_route_print(stdout);

}



