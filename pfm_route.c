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

extern struct rte_lpm_config pfm_lpm_config;
static int entry_no = 0;
static struct route_table{
	struct rte_lpm *lpm_mapper = rte_lpm_create(PFM_ROUTE_LPM_NAME,
						    rte_socket_id(),
						    &pfm_lpm_config);
	route_t entries[PFM_ROUTE_LPM_MAX_ENTRIES];
} ROUTE;



int 
route_add(uint16_t link_id,ipv4_addr_t net_mask,uint8_t net_mask_depth,ipv4_addr_t gateway_addr)	{
	int key,ret;

	// Check if netmask depth combination is present in LPM
	
	ret = rte_lpm_is_rule_present(ROUTE.lpm_mapper,
				      (uint32_t)net_mask.byte_addr,
				      net_mask_depth,
				      &key);
	if (ret)	{
	// If present just update the routing table
	
		ROUTE.entries[key].net_mask 		= (uint32_t)net_mask.byte_addr;
		ROUTE.entries[key].net_mask_depth 	= net_mask_depth;
		ROUTE.entries[key].link_id		= link_id;
		ROUTE.entries[key].gateway_addr		= (uint32_t)gateway_addr.byte_addr;

		pfm_trace_msg("Entry change in route table \nnet_mask :: %d:%d:%d:%d"
			      "\ndepth ::%d",
			      net_mask.byte_addr[0],
			      net_mask.byte_addr[1],
			      net_mask.byte_addr[2],
			      net_mask.byte_addr[3],
			      net_mask_depth);
		return 0;
	}
	
	// If not present check if space left in table
	
	else if (entry_no < PFM_ROUTE_LPM_MAX_ENTRIES)	{
	
	// If space available fill table
		entry_no++;
		ROUTE.entries[entry_no].net_mask 	= (uint32_t)net_mask.byte_addr;
		ROUTE.entries[entry_no].net_mask_depth 	= net_mask_depth;
		ROUTE.entries[entry_no].link_id		= link_id;
		ROUTE.entries[entry_no].gateway_addr	= (uint32_t)gateway_addr.byte_addr;

	// make LPM entry 
	
		ret = rte_lpm_add(ROUTE.lpm_mapper,
				  (uint32_t)net_mask.byte_addr,
				  net_mask_depth,
				  entry_no);
		if (ret < 0)	{
			pfm_log_msg(PFM_LOG_CRIT,
				    "Error in lpm entry")
			return ret;
		}

		pfm_trace_msg("Entry added in route table \nnet_mask :: %d:%d:%d:%d"
			      "\ndepth ::%d",
			      net_mask.byte_addr[0],
			      net_mask.byte_addr[1],
			      net_mask.byte_addr[2],
			      net_mask.byte_addr[3],
			      net_mask_depth);
		return 0;
	}

	else {
		pfm_log_msg("Error adding route to table");
		return -ENOMEM;
	}
}


route_t* route_query(ipv4_addr_t ip_addr)	{
	int ret,key;
	ret = rte_lpm_lookup(ROUTE.lpm_mapper,
			     (uint32_t)ip_addr.byte_addr;
			     &key);
	if (ret = 0)	{
		return ROUTE.entries[key];
	}

	if (ret = -ENOENT )	{
		pfm_trace_msg("Entry not found");
		return NULL;
	}

	else {
		pfm_log_msg(PFM_LOG_ERR,
			    "Error in route_query");
		return NULL;
	}
}

void route_print(const FILE *fp)	{
	if (fp == NULL)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid file stream provided");
		return
	}
	pfm_trace_msg("printing route table");
	for (int i = 0;i < entry_no;i++)	{
		fprintf("\n|Network mask :: %d:%d:%d:%d   |"
			"|Depth :: %d   |"
			"|Gateway address :: %d:%d:%d:%d   |",
			ROUTE.entries.net_mask.byte_addr[0],
			ROUTE.entries.net_mask.byte_addr[1],
			ROUTE.entries.net_mask.byte_addr[2],
			ROUTE.entries.net_mask.byte_addr[3],
			ROUTE.entries.net_mask_depth,
			ROUTE.entries.gateway_addr.byte_addr[0],
			ROUTE.entries.gateway_addr.byte_addr[1],
			ROUTE.entries.gateway_addr.byte_addr[2],
			ROUTE.entries.gateway_addr.byte_addr[3]);
	}
	return;
}



	





