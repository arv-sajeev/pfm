#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "pfm_route.h"
#include "e1ap_comm.h"
#include "e1ap_bearer_setup.h"
#include <rte_hash.h>
#include <rte_jhash.h>
#include <string.h>

#define PFM_TUNNEL_HASH_NAME "TUNNEL_TABLE_HASH"

static tunnel_t tunnel_table_g[MAX_TUNNEL_COUNT];
static uint32_t last_allocated_slot_g = 0;
static struct rte_hash *tunnel_hashtable_g;
static pfm_bool_t tunnel_table_up = PFM_FALSE;


static uint32_t cu_pdus_id = 0;
static uint32_t cu_drb_id  = 0;

// TD implement a better way to assign identifiers using hash probably
pfm_retval_t
tunnel_key_allocate(tunnel_key_t* tunnel_key,tunnel_type_t ttype,void * req)
{
	route_t* route_entry;
	switch (ttype)
	{
		case TUNNEL_TYPE_PDUS:
			// Get the local IP address for the given pdus_ul_ip_addr
			route_entry = pfm_route_query(((pdus_setup_req_info_t*)req)->pdus_ul_ip_addr);
			if (route_entry ==  NULL)
				return PFM_FAILED;
			tunnel_key->ip_addr = route_entry->gateway_addr;
			// TD Assign the te_id  in a better way
			tunnel_key->te_id = cu_pdus_id++;
			return PFM_SUCCESS;
			break;
		case TUNNEL_TYPE_DRB:
			// TD finding F1u address
			tunnel_key->ip_addr = pfm_str2ip("192.168.0.1");
			tunnel_key->te_id   = cu_drb_id++;
			return PFM_SUCCESS;
			break;
		default : return PFM_FAILED;
			  break;
	}
}

static pfm_retval_t 
tunnel_table_init(void)	
{
	pfm_trace_msg("Initializing tunnel_table");
	

	// Initialize the hash table
	struct rte_hash_parameters hash_params = 
	{
		.name			=	PFM_TUNNEL_HASH_NAME,
		.entries 		= 	MAX_TUNNEL_COUNT,
		.reserved		= 	0,
		.key_len		=	sizeof(tunnel_t),
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};
	tunnel_hashtable_g	=	rte_hash_create(&hash_params);
	if (tunnel_hashtable_g == NULL)	
	{
		pfm_log_msg(PFM_LOG_ALERT,"Error during rte_hash_create()");
		return PFM_FAILED;
	}

	// Clear the tunnel entries
	for (int i = 0;i < MAX_TUNNEL_COUNT;i++)
	{
		tunnel_table_g[i].is_row_used = PFM_FALSE;
	}
	tunnel_table_up = PFM_TRUE;
	return PFM_SUCCESS;
}

const tunnel_t*
tunnel_get(tunnel_key_t *key)
{
	int ret;	
	tunnel_t *entry;
	// If tunnel_table not up ERROR
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel table uninitialized");
		return NULL;
	}
	// Lookup for key in hash table 
	ret = rte_hash_lookup_data(tunnel_hashtable_g,key,(void **)&entry);
	
	// If hash hits return value 
	if (ret == 0)	
		return entry;
	
	// Log reason for miss
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
			pfm_trace_msg("tunnel_entry not found"); 
		if (ret == -EINVAL)
			pfm_log_msg(PFM_LOG_ERR,"invalid arguments for tunnel_get");
	}
	return NULL;
}

tunnel_t *      
tunnel_add(tunnel_key_t *key)
{
	int ret;	
	uint32_t i;
	tunnel_t *tunnel_entry;

	// Check if table is up, else initialize it 
	if (tunnel_table_up == PFM_FALSE)	
	{
		ret = tunnel_table_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,"Failed to init tunnel_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised tunnel_table");
	}

	// Check if entry already exists, return entry 
	ret = rte_hash_lookup_data(tunnel_hashtable_g,key,(void **)&tunnel_entry);
	if (ret == 0)
		return tunnel_entry;

	// If not found allocate an empty slot from table 
	for (i = last_allocated_slot_g + 1;i != last_allocated_slot_g;i++)
	{	
		if (i >= MAX_TUNNEL_COUNT)
			i = 0;
		if (!(tunnel_table_g[i].is_row_used)) 
		{
			tunnel_table_g[i].is_row_used 	= PFM_TRUE;
			tunnel_table_g[i].key.ip_addr 	= key->ip_addr;
			tunnel_table_g[i].key.te_id	= key->te_id;
			last_allocated_slot_g = i;
			pfm_trace_msg("New tunnel entry for tunnel_add");
			return &(tunnel_table_g[i]);
		}
	}
	// If no free entries return NULL
	pfm_log_msg(PFM_LOG_ERR,"tunnel_table is full");
	return NULL;
}

pfm_retval_t     
tunnel_remove(tunnel_key_t *key)
{
	int ret;	
	tunnel_t * entry;
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_table uninitialized");
		return PFM_FAILED;
	}
	
	// Get the tunnel entry
	ret = rte_hash_lookup_data(tunnel_hashtable_g,key,(void **)&entry);

	// Handle if no entry present 
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{
			pfm_log_msg(PFM_LOG_ERR,"entry not in tunnel_table");						
			return PFM_NOT_FOUND;
		}
		pfm_log_msg(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return PFM_FAILED;	
	}

	// Clear the entry and delete hash key
	entry->is_row_used = PFM_FALSE;

	return PFM_SUCCESS;
}

tunnel_t *      
tunnel_modify(tunnel_key_t *key)
{
	int ret;	
	uint32_t i;
	tunnel_t* entry;
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_table uninitialized");
		return NULL;
	}
	// Check if an instance with this key already exists
	ret = rte_hash_lookup_data(tunnel_hashtable_g,key,(void **)&entry);

	if (ret != 0)	
	{
		if (ret == -ENOENT)
			pfm_log_msg(PFM_LOG_ERR,"Attempt to modify entry that doesn't exist");
		else 
			pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return NULL;
	}
	
	for (i = last_allocated_slot_g + 1;i != last_allocated_slot_g;i++)
	{
		if (i >= MAX_TUNNEL_COUNT)
			i = 0;
		if (!(tunnel_table_g[i].is_row_used)) 
		{
			tunnel_table_g[i].is_row_used	=	PFM_TRUE;
			tunnel_table_g[i].key.ip_addr	=	key->ip_addr;
			tunnel_table_g[i].key.te_id	=	key->te_id;
			last_allocated_slot_g		=	i;
			pfm_trace_msg("New tunnel entry for tunnel_modify");
			return &(tunnel_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ERR,"tunnel_table is full");
	return NULL;
}

pfm_retval_t 
tunnel_commit(tunnel_t* nt)
{
	int ret;
	tunnel_t *entry;
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_table uninitialized");
		return PFM_FAILED;
	}

	if (nt == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid tunnel table entry pointer");
		return PFM_FAILED;
	}
	//Look if an entry exists with same key
	ret = rte_hash_lookup_data(tunnel_hashtable_g,&nt->key,(void **)&entry);
	// If we don't find an entry with nt's key
	if (ret < 0)
	{
		if (ret == -EINVAL)
		{
			pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		}
		// There wasn't an entry before this so this is a new entry just add new
		if (ret == -ENOENT)
		{
			ret = rte_hash_add_key_data(tunnel_hashtable_g,
						    &(nt->key),
						    nt);
			if (ret < 0)
			{
				pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_add_key_data() failed");
				return PFM_FAILED;
			}
			return PFM_SUCCESS;
		}
		return PFM_FAILED;
	}
	// if Entry found is the same as the one we're about commit
	if (nt == entry)
	{
		// If tunnel_remove was called on nt before commit delete the hash key
		if (!(entry->is_row_used))
		{
			ret = rte_hash_del_key(tunnel_hashtable_g,&(nt->key));
			if (ret != 0)
			{
				pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_del_key failed");
				return PFM_FAILED;
			}
			return PFM_SUCCESS;
		}
		return PFM_SUCCESS;
	}
		
	// nt and entry are different i.e the entry has been modified
	entry->is_row_used = PFM_FALSE;
	ret = rte_hash_add_key_data(tunnel_hashtable_g,
				    &(nt->key),
				    nt);
	if (ret != 0)
	{
		pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_add_key_data() failed");
		return PFM_FAILED;
	}
	return PFM_SUCCESS;
	
}

void   
tunnel_print_show(FILE *fp, tunnel_key_t *key)
{
	int ret;	
	tunnel_t* entry;
	char tunnel_ip_r[STR_IP_ADDR_SIZE],tunnel_ip_l[STR_IP_ADDR_SIZE];
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_table uninitialized");
		return;
	}
	
	ret = rte_hash_lookup_data(tunnel_hashtable_g,key,(void **)&entry);
	if (ret < 0)	
	{
		if (ret == -ENOENT)  
			pfm_log_msg(PFM_LOG_ERR,"entry not in tunnel_table");
		if (ret == -EINVAL)
			pfm_log_msg(PFM_LOG_ERR,"invalid arguments for tunnel_print_show");
		return;
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
printf("TODO: type to be used Type=%d\n",type);
	uint32_t hash_count,pos = 0;
	tunnel_key_t* key_ptr;
	tunnel_t *entry;
	
	
	char tunnel_ip_r[STR_IP_ADDR_SIZE],tunnel_ip_l[STR_IP_ADDR_SIZE];	
	if (tunnel_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_table uninitialized");
		return;
	}
	hash_count = rte_hash_count(tunnel_hashtable_g);
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
	
	while(rte_hash_iterate(tunnel_hashtable_g,(const void **)&key_ptr,(void **)&entry,&pos) >= 0)	
	{
		fprintf(fp,"%-16s | %-6d | %-16s | %-11d\n",
			pfm_ip2str(entry->key.ip_addr,tunnel_ip_l),
			entry->key.te_id,
			pfm_ip2str(entry->remote_ip,tunnel_ip_r),
			entry->tunnel_type);
   	}
	return;
}

