#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include <rte_hash.h>
#include <rte_jhash.h>
#include <string.h>

#define PFM_TUNNEL_HASH_NAME "TUNNEL_TABLE_HASH"
#define PFM_TUNNEL_TABLE_ENTRIES 32
#define PFM_TUNNEL_HASH_KEY_LEN 2

static tunnel_t tunnel_table_g[MAX_TUNNEL_COUNT];
static uint32_t last_allocated_slot_g = 0;
static struct rte_hash *hash_mapper;
static pfm_bool_t hash_up = PFM_FALSE;
/*
Using pointer to a constant concept here to make sure changes are not made to the tunnel_entry returned from tunnel_get
*/

static int 
hash_init(void)	
{
	if (hash_up != PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "ARP table already initialised");
	}

	struct rte_hash_parameters hash_params = 
	{
		.name			=	PFM_TUNNEL_HASH_NAME,
		.entries 		= 	PFM_TUNNEL_TABLE_ENTRIES,
		.reserved		= 	0,
		.key_len		=	PFM_TUNNEL_HASH_KEY_LEN,
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};

	hash_mapper	=	rte_hash_create(&hash_params);
	if (hash_mapper == NULL)	
	{
		pfm_log_msg(PFM_LOG_ALERT,
			    "Error during arp init");
		return -1;
	}
	return 0;


}



const tunnel_t*
tunnel_get(tunnel_key_t *key)
{
	int ret;	
	tunnel_t *entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	ret = rte_hash_lookup_data(hash_mapper,
				   (void *)key,
				   (void *)entry);
	if (ret == 0)	{
		return entry;
	}
	
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in tunnel_table");			
		}
		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "invalid arguments for tunnel_get");
		}
	}
	
	return NULL;
	
}

tunnel_t *      
tunnel_add(tunnel_key_t *key)
{
	int ret;	
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	if (key == NULL)	
	{
		return NULL;
	}
	// Circular queue style search for empty entry 
	for (uint32_t i =  (last_allocated_slot_g+1)%PFM_TUNNEL_TABLE_ENTRIES;
		i != last_allocated_slot_g;
		i = (i+1)%PFM_TUNNEL_TABLE_ENTRIES)
	{	
		if (tunnel_table_g[i].is_row_used == 0)
		{
			tunnel_table_g[i].is_row_used 	= 1;
			tunnel_table_g[i].key.ip_addr 	= key->ip_addr;
			tunnel_table_g[i].key.te_id	= key->te_id;
			last_allocated_slot_g = i;
			pfm_trace_msg("New tunnel entry for tunnel_add");
			return &(tunnel_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ERR,
		    "tunnel_table is full");
	return NULL;
}

pfm_retval_t     
tunnel_remove(tunnel_key_t *key)
{
	int ret;	
	tunnel_t * entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return PFM_FAILED;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	
	// Get the tunnel entry
	ret = rte_hash_lookup_data(hash_mapper,
				   (void *) key,
				   (void *) entry);
	// Handle if not present 
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in tunnel_table");			
		}
		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "invalid arguments for tunnel_remove");
		}
		return PFM_FAILED;	
	}
	// Clear the entry 
	memset(entry,0,sizeof(tunnel_t));
	//delete the key
	ret = rte_hash_del_key(hash_mapper,
			       (void *)key);

	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in tunnel_table");			
		}
		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "invalid arguments for tunnel_remove");
		}
		return PFM_FAILED;	
	}
	return PFM_SUCCESS;
}

tunnel_t *      
tunnel_modify(tunnel_key_t *key)
{
	int ret;	
	tunnel_t* entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}

	ret = rte_hash_lookup_data(hash_mapper,
				   (void *)key,
				   (void *)entry);
	if (ret == 0)	
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Attempting to modify entry that doesn't exist");
		return NULL;
	}
	
	for (uint32_t i = (last_allocated_slot_g+1)%PFM_TUNNEL_TABLE_ENTRIES;
		i != last_allocated_slot_g;
		i = (i+1)%PFM_TUNNEL_TABLE_ENTRIES)
	{
		if (tunnel_table_g[i].is_row_used == 0)
		{
			tunnel_table_g[i].is_row_used	=	1;
			tunnel_table_g[i].key.ip_addr	=	key->ip_addr;
			tunnel_table_g[i].key.te_id	=	key->te_id;
			last_allocated_slot_g		=	i;
			pfm_trace_msg("New tunnel entry for tunnel_modify");
			return &(tunnel_table_g[i]);
		}
	}
	return NULL;
}

pfm_retval_t 
tunnel_commit(tunnel_t* nt)
{
	int ret;	
	tunnel_t *entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return PFM_FAILED;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	if (nt == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid tunnel table entry pointer");
	}
	
	ret == rte_hash_lookup_data(hash_mapper,
				    (void *)&nt->key,
				    (void *)entry);
	if (ret >= 0)
	{
		memset(entry,0,sizeof(tunnel_t));
	}
	
	// Insert the tunnel_t entry into the hash table 
	ret = rte_hash_add_key_data(hash_mapper,
				   (void *)&nt->key,
				   (void *)nt);
	if (ret == 0)
		return PFM_SUCCESS;
	if (ret == -EINVAL)
	{
		pfm_log_msg(PFM_LOG_ERR,
		            "incorrect parameters tunnel_commit");
		return PFM_FAILED;
	}
	if (ret == -ENOSPC)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "tunnel_table is full");
		return PFM_FAILED;
	}
	return PFM_FAILED;
}

void   
tunnel_print_show(FILE *fp, tunnel_key_t *key)
{
	int ret;	
	tunnel_t* entry;
	char tunnel_ip_r[STR_IP_ADDR_SIZE],tunnel_ip_l[STR_IP_ADDR_SIZE];
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return ;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	if (fp == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid log error");
		return ;
	}
	
	ret = rte_hash_lookup_data(hash_mapper,
				   (void *)key,
				   (void *)entry);
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in tunnel_table");			
		}
		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "invalid arguments for tunnel_print_show");
		}
		return ;
	}
	switch(entry->tunnel_type)	
	{
		case TUNNEL_TYPE_PDUS:

				fprintf(fp,"Route list\n"
					   "Tunnel local ip  :   %-16s\n"
					   "Tunnel id        :   %-16d\n"
					   "Tunnel remote ip :   %-16s\n"
					   "Tunnel type	     :   %-16s\n"
					   "PDUS id	     :   %-16d\n"
					   "Flow count	     :   %-16d\n"
					   "\n\nFlows\n"
					   "%-7s %-6s %-6s\n",
					   pfm_ip2str(entry->key.ip_addr,tunnel_ip_l),
					   entry->key.te_id,
					   pfm_ip2str(entry->remote_ip,tunnel_ip_r),
					   "PDU",
					   entry->pdus_info.pdus_id,
					   entry->pdus_info.flow_count,
					   "Flow id",
					   "Status",
				           "DRB id");
				for (int i = 0;i < entry->pdus_info.flow_count;i++)
				{
					fprintf(fp,"%-7d %-6d %-6d",
					    entry->pdus_info.flow_list[i].flow_id,
					    entry->pdus_info.flow_list[i].r_qos_status,
					    entry->pdus_info.flow_list[i].mapped_drb_idx);
				}
				break;

		case TUNNEL_TYPE_DRB:


				fprintf(fp,"Route list\n"
					   "Tunnel local ip  		:   %-16s\n"
					   "Tunnel id       		:   %-16d\n"
					   "Tunnel remote ip 		:   %-16s\n"
					   "Tunnel type	     		:   %-16s\n"
					   "\n\n DRB info\n"
				           "DRB id	     		:   %-16d\n"
					   "is_default	     		:   %-16d\n"
					   "is_dl_sdap_hdr_enabled	:   %-16d\n"
					   "is_ul_sdap_hdr_enabled	:   %-16d\n"
					   "mapped_flow_idx		:   %-16d\n"
					   "mapped_pdus_idx		:   %-16d\n",
					   pfm_ip2str(entry->key.ip_addr,tunnel_ip_l),
					   entry->key.te_id,
					   pfm_ip2str(entry->remote_ip,tunnel_ip_r),
					   "DRB",
					   entry->drb_info.drb_id,
					   entry->drb_info.is_default,
					   entry->drb_info.is_dl_sdap_hdr_enabled,
					   entry->drb_info.is_ul_sdap_hdr_enabled,
					   entry->drb_info.mapped_flow_idx,
					   entry->drb_info.mapped_pdus_idx);
				break;	
	}
		
	return;
}

void        
tunnel_print_list(FILE *fp, tunnel_type_t type)
{
	int ret;
	uint32_t hash_count,pos = 0;
	const void* key_ptr;
	void  *data_ptr;
	
	
	char tunnel_ip_r[STR_IP_ADDR_SIZE],tunnel_ip_l[STR_IP_ADDR_SIZE];	
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init tunnel_table ");
			return ;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}
	hash_count = rte_hash_count(hash_mapper);
	if (hash_count == 0)	
	{
		fprintf(fp,"Tunnel table is empty\n");
		return;
	}
	
	fprintf(fp,"\nTunnel list\n"
		   "%-16s | %-6s | %-16s | %-11s\n",
		   "Local ip addr",
		   "tun id",
		   "Remote ip addr",
		   "Tunnel type");
	
	while(rte_hash_iterate(hash_mapper,&key_ptr,&data_ptr,&pos) >= 0)	
	{
		tunnel_t *entry = (tunnel_t *)data_ptr;
		fprintf(fp,"%-16s | %-6d | %-16s | %-11d\n",
			pfm_ip2str(entry->key.ip_addr,tunnel_ip_l),
			entry->key.te_id,
			pfm_ip2str(entry->remote_ip,tunnel_ip_r),
			entry->tunnel_type);
   	}
	return;
}

