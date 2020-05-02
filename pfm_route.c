#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>
#include <errno.h>
#include <rte_lpm.h>

#include "pfm.h"
#include "pfm_log.h"
#include "pfm_comm.h"
#include "pfm_route.h"
#include "pfm_utils.h"

#define PFM_ROUTE_LPM_MAX_ENTRIES       32
#define PFM_ROUTE_LPM_MAX_TBL8S         8	// PFM_ROUTE_LPM_MAX_ENTRIES /4 is recommended
#define PFM_ROUTE_LPM_FLAGS             0
#define PFM_ROUTE_LPM_NAME              "PFM_ROUTE_LPM"
#define PFM_ROUTE_NAME_SIZE		32

static uint32_t entry_count = 0;
static pfm_bool_t lpm_up = PFM_FALSE;
static route_t route_table[PFM_ROUTE_LPM_MAX_ENTRIES]; 
static struct rte_lpm *lpm_mapper;

int 
lpm_init(void)	{
	if (lpm_up != PFM_FALSE)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "LPM already initialised");
	}

	struct  rte_lpm_config lpm_config;
	struct rte_lpm_config pfm_lpm_config = {

        .max_rules      =       PFM_ROUTE_LPM_MAX_ENTRIES,
        .number_tbl8s   =       PFM_ROUTE_LPM_MAX_TBL8S,
        .flags          =       PFM_ROUTE_LPM_FLAGS
	};

	lpm_mapper = rte_lpm_create(PFM_ROUTE_LPM_NAME,
						rte_socket_id(),
						&pfm_lpm_config);
	if (lpm_mapper == NULL)	{
		pfm_log_msg(PFM_LOG_ALERT,
			    "Failed to init lpm");
		return -1;
	}
	lpm_up = PFM_TRUE;
	return 0;
}



int 
pfm_route_add(ipv4_addr_t net_mask,uint8_t net_mask_depth,ipv4_addr_t gateway_addr,uint16_t link_id)	{
	int ret;
	uint32_t key;
	if (lpm_up == PFM_FALSE)	
	{
		pfm_trace_msg("First call to route_add");
		lpm_init();
		pfm_trace_msg("Intialised lpm module");

	}

	// Check if netmask depth combination is present in LPM
	
	ret = rte_lpm_is_rule_present(lpm_mapper,
				      net_mask,
				      net_mask_depth,
				      &key);
	if (ret)	{
	// If present just update the routing table
	// Pending convert 
	}
	
	// If not present check if space left in table
	
	else if (entry_count < PFM_ROUTE_LPM_MAX_ENTRIES)	{
	
	// If space available fill table
		key = entry_count;
		ret = rte_lpm_add(lpm_mapper,
				  net_mask,
				  net_mask_depth,
				  key);
		if (ret < 0)	{
			pfm_log_msg(PFM_LOG_CRIT,
				    "Error in lpm entry");
			return ret;
		}
		//Pending
		entry_count++;
	}
	else {
		pfm_log_msg(PFM_LOG_WARNING,
			    "Error adding route to table");
		return -ENOMEM;
	}
	route_table[key].net_mask		= net_mask;
	route_table[key].net_mask_depth 	= net_mask_depth;
	route_table[key].link_id		= link_id;
	route_table[key].gateway_addr		= gateway_addr;
	return 0;
}


route_t* 
pfm_route_query(ipv4_addr_t ip_addr)	{
	int ret;
	uint32_t key;
	ret = rte_lpm_lookup(lpm_mapper,
			     ip_addr,
			     &key);
	if (ret == 0 )	{
		return &(route_table[key]);
	}

	else if (ret == -ENOENT )	{
		pfm_trace_msg("Entry not found");
		return NULL;
	}

	else {
		pfm_log_msg(PFM_LOG_ERR,
			    "Error in route_query");
		return NULL;
	}
}

void 
pfm_route_print(FILE *fp)	{
	char net_mask[PFM_ROUTE_NAME_SIZE];
	char gateway_addr[PFM_ROUTE_NAME_SIZE];
	if (fp == NULL)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid file stream provided");
		return;
	}
	pfm_trace_msg("printing route table");
	if (entry_count == 0)	{
		fprintf(fp,"Routing table is empty");
		return;
	}


	fprintf(fp,"\n---------------------------ROUTING TABLE---------------------------\n");
	fprintf(fp,"|  %-20s |  %-20s |  %-5s |  %-5s |\n",
		"Network mask","Gateway address","depth","link");
	for (uint16_t i = 0;i < entry_count;i++)	{
		fprintf(fp,"|  %-20s |  %-20s |  %-5d |  %-5d |\n",
			pfm_ip2str(route_table[i].net_mask,net_mask),
			pfm_ip2str(route_table[i].gateway_addr,gateway_addr),
			route_table[i].net_mask_depth,
			route_table[i].link_id);
	}
	fprintf(fp,"-------------------------------------------------------------------\n");
		    
}



	





