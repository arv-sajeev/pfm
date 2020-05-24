#include <rte_hash.h>
#include <rte_jhash.h>
#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include <string.h>

#define PFM_UE_CTX_TABLE_NAME "UE_CTX_TABLE_HASH"


static ue_ctx_t ue_ctx_table_g[MAX_UE_COUNT];
static uint32_t last_allocated_slot_g = 0;
static struct rte_hash* hash_mapper;
static pfm_bool_t hash_up = PFM_FALSE;
static pfm_bool_t ue_ctx_table_up = PFM_FALSE;

static ue_id = 0;

uint32_t
ue_id_allocate()
{
	// TD FInd a better a way to do this 
	return ue_id++;
}



static pfm_retval_t 
ue_ctx_table_init(void)	
{

	struct rte_hash_parameters hash_params = 
	{
		.name			=	PFM_UE_CTX_TABLE_NAME,
		.entries 		= 	MAX_UE_COUNT,
		.reserved		= 	0,
		.key_len		=	sizeof(uint32_t),
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};

	hash_mapper	=	rte_hash_create(&hash_params);
	if (hash_mapper == NULL)	
	{
		pfm_log_msg(PFM_LOG_ALERT,
			    "Error during arp init");
		return PFM_FAILED;
	}

	for (int i = 0;i < MAX_UE_COUNT;i++)
	{
		ue_ctx_table_g[i].is_row_used = PFM_FALSE;
	}
	
	ue_ctx_table_up = PFM_TRUE;
	return PFM_SUCCESS;
}



ue_ctx_t *
ue_ctx_add(uint32_t ue_id)
{
	int ret;	
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}
	// Circular queue style search for empty entry 
	for (uint32_t i =  (last_allocated_slot_g+1)%PFM_UE_CTX_TABLE_ENTRIES;
		i != last_allocated_slot_g;
		i = (i+1)%PFM_UE_CTX_TABLE_ENTRIES)
	{	
		if (ue_ctx_table_g[i].is_row_used == 0)
		{
			ue_ctx_table_g[i].cuup_ue_id = ue_id;
			ue_ctx_table_g[i].is_row_used = 1;
			last_allocated_slot_g = i;
			return &(ue_ctx_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ERR,
		    "ue_ctx_table is full");
	return NULL;

}

pfm_retval_t	ue_ctx_remove(uint32_t ue_id)
{
	int ret;	
	ue_ctx_t * entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return PFM_FAILED;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}
	
	// Get the tunnel entry
	ret = rte_hash_lookup_data(hash_mapper,
				   (void *)&ue_id,
				   (void *)entry);
	// Handle if not present 
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in ue_ctx_table");			
		}
		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "invalid arguments for tunnel_remove");
		}
		return PFM_FAILED;	
	}
	//delete the key
	ret = rte_hash_del_key(hash_mapper,
			       (void *)&ue_id);
	// Clear tunnel lists
	for (int i = 0;i < entry->pdus_count ; i++)
	{
		pfm_retval_t r = tunnel_remove(&((entry->pdus_tunnel_list[i])->key));
		if (r == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,
				"Error clearing pdus entries");
			return PFM_FAILED;
		}
	}
	
	for (int i = 0;i < entry->drb_count ; i++)
	{
		pfm_retval_t r = tunnel_remove(&((entry->drb_tunnel_list[i])->key));
		if (r == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,
				"Error drb clearing entries");
			return PFM_FAILED;
		}
	}
	// Clear entry
		
		
	memset(entry,0,sizeof(ue_ctx_t));

	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{ 
			pfm_log_msg(PFM_LOG_ERR,
				    "entry not in ue_ctx_table");			
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

ue_ctx_t *	ue_ctx_modify(uint32_t ue_id)
{
	int ret;	
	ue_ctx_t* entry;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}
	
	// Check if an instance with this ue_id already exists
	ret = rte_hash_lookup_data(hash_mapper,
				   (void *)&ue_id,
				   (void *)&entry);
	// If it doesn't exist it is an invalid request
	if (ret != 0)	
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Attempting to modify an entry that doesn't exist");
		return NULL;

	}
	
	// Circular queue style search for empty entry 
	for (uint32_t i =  (last_allocated_slot_g+1)%PFM_UE_CTX_TABLE_ENTRIES;
		i != last_allocated_slot_g;
		i = (i+1)%PFM_UE_CTX_TABLE_ENTRIES)
	{	
		if (ue_ctx_table_g[i].is_row_used == 0)
		{
			ue_ctx_table_g[i].cuup_ue_id = ue_id;
			last_allocated_slot_g = i;
			return &(ue_ctx_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ERR,
		    "ue_ctx_table is full");
	
	return NULL;
}

pfm_retval_t	ue_ctx_commit(ue_ctx_t *new_ctx)
{
	int ret;	
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return PFM_FAILED;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}
	if (new_ctx == NULL)	{
		pfm_log_msg(PFM_LOG_ERR,
				"Invalid ue_ctx pointer");
		return PFM_FAILED;
	}
	ue_ctx_t* entry;
	ret = rte_hash_lookup_data(hash_mapper,
					(void *)&(new_ctx->cuup_ue_id),
					(void*)entry);	
	if (ret >= 0)
	{
		memset(entry,0,sizeof(ue_ctx_t));

	}

	ret = rte_hash_add_key_data(hash_mapper,
				    (void *)&(new_ctx->cuup_ue_id),
				    (void *)new_ctx);	
	if (ret == 0)
		return PFM_SUCCESS;
	if (ret == -EINVAL)	
	{
		pfm_log_msg(PFM_LOG_ERR,
				"Invalid parameter to ue_ctx_commit");
		return PFM_FAILED;
	}	

	if (ret == -ENOSPC)
	{
		pfm_log_msg(PFM_LOG_ERR,
				"ue_ctx_table is full");
		return PFM_FAILED;
	}	
	return PFM_FAILED;
}

const ue_ctx_t *
ue_ctx_get(uint32_t ue_id)
{
	int ret;	
	ue_ctx_t* entry;
	if (hash_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table uninitialize");
		return NULL;
	}
	ret = rte_hash_lookup_data(hash_mapper,ue_id,&entry);
	if (ret == 0)	
		return entry;
	if (ret < 0)	
	{
		if (ret == -ENOENT)	
			pfm_trace_msg("No entry found in ue_ctx_table");
		if (ret == -EINVAL)	
			pfm_log_msg(PFM_LOG_ERR,"Invalid parameters in ue_ctx_table");
	}	
	return NULL;
}

void	
ue_ctx_print_list(FILE *fp)
{	
	const uint32_t *key_ptr;
	ue_ctx_t * data_ptr;
	uint32_t ptr = 0,hash_count;
	int pos,ret;
	if (hash_up == PFM_FALSE)	
	{
		ret = hash_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return;
		}
	}
	if (fp == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid file pointer");
	}
	hash_count = rte_hash_count(hash_mapper);
	if (hash_count == 0)	{
		fprintf(fp,"ue_ctx_table table is empty\n");
		return;
	}
	
	fprintf(fp,"\n %-10s | %-10s | %-10s | %-10s\n",
		"CUUP-UEid","CUCP-UEid","DRB count","PDUS count");
	while (rte_hash_iterate(hash_mapper,(void *)&key_ptr,(void *)&data_ptr,&ptr) >=0)	{
		fprintf(fp," %-10d | %-10d | %-10d | %-10d\n",
			data_ptr->cuup_ue_id,
			data_ptr->cucp_ue_id,
			data_ptr->drb_count,
			data_ptr->pdus_count);
	}
	return;
}

void		ue_ctx_print_show(FILE *fp, uint32_t ue_id)
{
	printf("ue_ctx_print_show(fp=%p,ueId=%d) invoked. "
		"But not implemented\n",
		fp,ue_id);
	return;
}


