#include <stdio.h>                                               
#include <stdlib.h>                                                                                 
#include <signal.h>       
#include <rte_common.h>

#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm_route.h"
#include "pfm_utils.h"



int main(int argc,char* argv[])	{
	pfm_retval_t ret_val;
	int ret;
	int lcore_id;
	ipv4_addr_t ip,gw;
	char ipv4_addr[IP_ADDR_SIZE];

	pfm_log_open("ARP-UNIT-TEST",PFM_LOG_DEBUG);	
	ret = rte_eal_init(argc,argv);
	lpm_init();
	ip=pfm_str2ip("129.168.1.23");
	gw=pfm_str2ip("129.168.1.1");

	// T.1 Simple insert to table 
	pfm_route_add(ip, 24, gw, 7);
	pfm_route_print(stdout);

	// T.2 Update existing extry
	pfm_route_add(ip, 20, gw, 5);
	pfm_route_print(stdout);
	
	// T.3 DIfferent depth same net_mask is taken as new entry
	pfm_route_add(ip, 23, gw, 3);
	pfm_route_print(stdout);

	ip=pfm_str2ip("131.168.1.23");
	gw=pfm_str2ip("131.168.1.1");

	// T.4 Different IP and different depth taken as new entry
	pfm_route_add(ip, 12, gw, 3);
	pfm_route_print(stdout);

	// T.5 filling Table should fail after it crosses 32 entries
	ip = pfm_str2ip("192.168.22.1");
	for (int i = 1;i < 32;i++)	{
		if (i%8 == 0)	{
			pfm_route_add(ip,i,gw,7);
		}
		else {
			pfm_route_add(ip,i,gw,1);
		}
		pfm_route_print(stdout);
	}


	// T.6 testing pfm_route_query 
	ip = pfm_str2ip("129.168.1.123");

	route_t* route =  pfm_route_query(ip);
	if (route ==  NULL)	{
		printf("FAILED Query T.6.a\n");
	}
	else if (route->link_id ==  7)	{
		printf("PASSED\n");
	}

	else {
		printf("FAILED Query T.6.a\n");
	}

	ip = pfm_str2ip("9.9.9.9");
	route =  pfm_route_query(ip);

	if (route !=  NULL)	{
		printf("FAILED Query T.6.b\n");
		printf("%s\n",pfm_ip2str(route->net_mask,ipv4_addr));
	}

	else {
		printf("PASSED\n");
	}

	ip = pfm_str2ip("192.168.255.255");
	route =  pfm_route_query(ip);
	
	if (route ==  NULL)	{
		printf("FAILED Query T.6.c\n");
	}

	else if (route->link_id ==  7)	{
		printf("PASSED\n");
	}

	else {
		printf("FAILED Query T.6.c\n");
	}


}



