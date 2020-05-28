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
static struct rte_hash* ue_ctx_hashtable_g;
static pfm_bool_t ue_ctx_table_up = PFM_FALSE;

static int ue_id = 0;

uint32_t
ue_ctx_id_allocate(void)
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

	ue_ctx_hashtable_g	=	rte_hash_create(&hash_params);
	if (ue_ctx_hashtable_g == NULL)	
	{
		pfm_log_msg(PFM_LOG_ALERT,"Error during rte_hash_create()");
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
	uint32_t i;
	ue_ctx_t *entry;
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		ret = ue_ctx_table_init();
		if (ret != 0)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return NULL;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}
	// If entry already exists return entry 
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,&entry);
	if (ret == 0)
		return entry;
	// Circular queue style search for empty entry 
	for (i = last_allocated_slot_g + 1;i != last_allocated_slot_g;i++)
	{	
		if (i >= MAX_UE_COUNT)
			i = 0;
		if (!(ue_ctx_table_g[i].is_row_used))
		{
			ue_ctx_table_g[i].cuup_ue_id = ue_id;
			ue_ctx_table_g[i].is_row_used = 1;
			last_allocated_slot_g = i;
			return &(ue_ctx_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table is full");
	return NULL;
}

pfm_retval_t	
ue_ctx_remove(uint32_t ue_id)
{
	int ret,i;	
	ue_ctx_t * entry;
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"invoking ue_ctx_remove() before init");
		return PFM_FAILED;
	}
	
	// Get the tunnel entry
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,&entry);
	// Handle if not present 
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{
			pfm_log_msg(PFM_LOG_ERR,"entry not in ue_ctx_table");
			return PFM_NOT_FOUND;
		}
		pfm_log_msg(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return PFM_FAILED;	
	}
	entry->is_row_used = PFM_FALSE;
	
	// Clear tunnel lists
	for (i = 0;i < entry->pdus_count ; i++)
		 tunnel_remove(&((entry->pdus_tunnel_list[i])->key));
	for (i = 0;i < entry->drb_count ; i++)
		 tunnel_remove(&((entry->drb_tunnel_list[i])->key));
	return PFM_SUCCESS;
}

ue_ctx_t *	
ue_ctx_modify(uint32_t ue_id)
{
	int ret;	
	uint32_t i,prev;
	ue_ctx_t *entry,*new_ctx;
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"invoked ue_ctx_modify() before ue_ctx_table_init()");
		return NULL;
	}
	
	// Check if an instance with this ue_id already exists
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,&entry);
	// If it doesn't exist it is an invalid request
	if (ret != 0)	
	{
		if (ret == -ENOENT)
			pfm_log_msg(PFM_LOG_ERR,"Attempt to modify entry that doesn't exist");
		else
			pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return NULL;

	}
	
	// Circular queue style search for empty entry 
	prev = last_allocated_slot_g;	//Storing it to use in logic for whether the table is full
	for ( i = last_allocated_slot_g +1;i != last_allocated_slot_g;i++)  
	{	
		if ( i >=  MAX_UE_COUNT)
			i = 0;
		if (!(ue_ctx_table_g[i].is_row_used))
		{
			ue_ctx_table_g[i].cuup_ue_id 	= ue_id;
			ue_ctx_table_g[i].is_row_used 	= PFM_TRUE;
			last_allocated_slot_g = i;
			new_ctx = &(ue_ctx_table_g[i]);
			break;
		}
	}
	// Table has no free entries
	if (i == prev)
	{
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table is full");
		return NULL;
	}


	// Copy the pdus_list of the old entry to the new one so we don't lose track
	for (new_ctx->pdus_count = 0;new_ctx->pdus_count < entry->pdus_count;new_ctx->pdus_count++)
		new_ctx->pdus_tunnel_list[new_ctx->pdus_count] = entry->pdus_tunnel_list[i];


	// Copy the drb_list as well
	for (new_ctx->drb_count = 0;new_ctx->drb_count < entry->drb_count;new_ctx->drb_count++)
		new_ctx->drb_tunnel_list[new_ctx->drb_count] = entry->drb_tunnel_list[i];
	return new_ctx;
}

pfm_retval_t	
ue_ctx_commit(ue_ctx_t *new_ctx)
{
	int ret;	
	uint32_t i,j;
	ue_ctx_t* entry;
	tunnel_t* tunnel_entry;
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"invoking ue_ctx_commit() before init");
		return PFM_FAILED;
	}
	
	// Check ue_ctx
	if (new_ctx == NULL)	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid ue_ctx pointer");
		return PFM_FAILED;
	}

	// Commit drb tunnels
	for (i = 0;i < new_ctx->drb_count;i++)	
	{
		tunnel_entry 	= new_ctx->drb_tunnel_list[i];
		ret		= tunnel_commit(tunnel_entry);
	// If the tunnel_remove() was called on tunnel don't include in drb_tunnel_list
		if (!(tunnel_entry->is_row_used))
			new_ctx->drb_tunnel_list[i] = NULL;
	}

	// Pack the drb_tunnel_list removing the ones assigned as NULL
	for (i = 0,j = 0;i < new_ctx->drb_count;i++)
	{
		if (new_ctx->drb_tunnel_list[i])
		{
			new_ctx->drb_tunnel_list[j] = new_ctx->drb_tunnel_list[i];
			j++;
		}
	}
	new_ctx->drb_count = j;

	// Commit pdus tunnels
	for (i = 0;i < new_ctx->pdus_count;i++)
	{
		tunnel_entry	= new_ctx->pdus_tunnel_list[i];
		ret		= tunnel_commit(tunnel_entry);

		if (!(tunnel_entry->is_row_used))
			new_ctx->pdus_tunnel_list[i] = NULL;
	}

	// Pack the pdus_tunnel_list
	for (i = 0,j = 0;i < new_ctx->pdus_count;i++)
	{
		if (new_ctx->drb_tunnel_list[i])
		{
			new_ctx->pdus_tunnel_list[j] = new_ctx->drb_tunnel_list[i];
			j++;
		}
	}

	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,
				   &(new_ctx->cuup_ue_id),
				   &entry);
	// if we don't find an entry with new_ctx's cuup_ue_id
	if (ret < 0)
	{
		// Bad call to rte_hash_lookup_data
		if (ret == -EINVAL)
		{
			pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		}
		// There wasn't entry before so this is a new entry just add a new record
		if (ret == -ENOENT)	
		{
			ret = rte_hash_add_key_data(ue_ctx_hashtable_g,
						    &(new_ctx->cuup_ue_id),
						    new_ctx);
			if ( ret < 0)	
			{
				pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_add_key_data() failed");
				return PFM_FAILED;
			}
			return PFM_SUCCESS;
		}
		return PFM_FAILED;
	}
	
	else 
	{
		// If the entry found in table is same as the one we're about to commit
		if (entry ==  new_ctx)
		{
			// If it had ue_ctx_remove called on it is_row_used will be false
			if (!(entry->is_row_used))
			{
				//delete the key from ue_ctx_hashtable_g	
				ret = rte_hash_del_key(ue_ctx_table_g,&(new_ctx->cuup_ue_id));
				if (ret != 0)
				{
					pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_del_key fail");
					return PFM_FAILED;
				}
				return PFM_SUCCESS;
			}
			// Committing an existing commit no change required
			return PFM_SUCCESS;
		}

		else 
		{
			// the ue_ctx is modified so update the hash table and remove old entry
			entry->is_row_used = PFM_FALSE;
			ret = rte_hash_add_key_data(ue_ctx_hashtable_g,
						    &(new_ctx->cuup_ue_id),
						    new_ctx);
			if (ret != 0)
			{
				pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_add_key_data failed");
				return PFM_FAILED;
			}
			return PFM_SUCCESS;

		}
	}
	return PFM_SUCCESS;
}

const ue_ctx_t *
ue_ctx_get(uint32_t ue_id)
{
	int ret;	
	ue_ctx_t* entry;
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table uninitialize");
		return NULL;
	}
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,&entry);
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
	if (ue_ctx_table_up == PFM_FALSE)	
	{
		ret = ue_ctx_table_init();
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
	hash_count = rte_hash_count(ue_ctx_hashtable_g);
	if (hash_count == 0)	{
		fprintf(fp,"ue_ctx_table table is empty\n");
		return;
	}
	
	fprintf(fp,"\n %-10s | %-10s | %-10s | %-10s\n",
		"CUUP-UEid","CUCP-UEid","DRB count","PDUS count");
	while (rte_hash_iterate(ue_ctx_hashtable_g,(void *)&key_ptr,(void *)&data_ptr,&ptr) >=0)	{
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


