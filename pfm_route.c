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
static int lpm_in = 0;

static route_table_t route_table; 


int lpm_init(void)	{
	if (lpm_in !=0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "LPM already initialised");
	}
	lpm_in = 1;
	route_table.lpm_mapper = rte_lpm_create(PFM_ROUTE_LPM_NAME,
						rte_socket_id(),
						&pfm_lpm_config);
	if (route_table.lpm_mapper == NULL)	{
		pfm_log_msg(PFM_LOG_ALERT,
			    "Failed to init lpm");
		return -1;
	}
	return 0;
}



int 
route_add(uint16_t link_id,ipv4_addr_t *net_mask,uint8_t net_mask_depth,ipv4_addr_t *gateway_addr)	{
	int ret;
	uint32_t key;

	// Check if netmask depth combination is present in LPM
	
	ret = rte_lpm_is_rule_present(route_table.lpm_mapper,
				      (uint32_t)net_mask->addr_bytes,
				      net_mask_depth,
				      &key);
	if (ret)	{
	// If present just update the routing table
		pfm_trace_msg("Entry change in route table \nnet_mask :: %d:%d:%d:%d"
			      "\ndepth ::%d",
			      net_mask->addr_bytes[0],
			      net_mask->addr_bytes[1],
			      net_mask->addr_bytes[2],
			      net_mask->addr_bytes[3],
			      net_mask_depth);
	}
	
	// If not present check if space left in table
	
	else if (entry_no < PFM_ROUTE_LPM_MAX_ENTRIES)	{
	
	// If space available fill table
		entry_no++;	
		key = entry_no;
		ret = rte_lpm_add(route_table.lpm_mapper,
				  (uint32_t)net_mask->addr_bytes,
				  net_mask_depth,
				  key);
		if (ret < 0)	{
			pfm_log_msg(PFM_LOG_CRIT,
				    "Error in lpm entry");
			return ret;
		}
		pfm_trace_msg("Entry added in route table \nnet_mask :: %d:%d:%d:%d"
			      "\ndepth ::%d",
			      net_mask->addr_bytes[0],
			      net_mask->addr_bytes[1],
			      net_mask->addr_bytes[2],
			      net_mask->addr_bytes[3],
			      net_mask_depth);
	}
	else {
		pfm_log_msg(PFM_LOG_WARNING,
			    "Error adding route to table");
		return -ENOMEM;
	}
	ipv4_addr_copy(net_mask,&(route_table.entries[key].net_mask));
	route_table.entries[key].net_mask_depth 	= net_mask_depth;
	route_table.entries[key].link_id		= link_id;
	ipv4_addr_copy(gateway_addr,&(route_table.entries[key].gateway_addr));
	return 0;
}


route_t* route_query(ipv4_addr_t* ip_addr)	{
	int ret;
	uint32_t key;
	ret = rte_lpm_lookup(route_table.lpm_mapper,
			     (uint32_t)ip_addr->addr_bytes,
			     &key);
	if (ret == 0)	{
		return &(route_table.entries[key]);
	}

	if (ret == -ENOENT )	{
		pfm_trace_msg("Entry not found");
		return NULL;
	}

	else {
		pfm_log_msg(PFM_LOG_ERR,
			    "Error in route_query");
		return NULL;
	}
}

void route_print(FILE *fp)	{
	if (fp == NULL)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid file stream provided");
		return;
	}
	pfm_trace_msg("printing route table");
	for (int i = 0;i < entry_no;i++)	{
		fprintf(fp,"\n|Network mask :: %d:%d:%d:%d   |"
			"|Depth :: %d   |"
			"|Gateway address :: %d:%d:%d:%d   |",
			route_table.entries[entry_no].net_mask.addr_bytes[0],
			route_table.entries[entry_no].net_mask.addr_bytes[1],
			route_table.entries[entry_no].net_mask.addr_bytes[2],
			route_table.entries[entry_no].net_mask.addr_bytes[3],
			route_table.entries[entry_no].net_mask_depth,
			route_table.entries[entry_no].gateway_addr.addr_bytes[0],
			route_table.entries[entry_no].gateway_addr.addr_bytes[1],
			route_table.entries[entry_no].gateway_addr.addr_bytes[2],
			route_table.entries[entry_no].gateway_addr.addr_bytes[3]);
	}
	return;
}



	





