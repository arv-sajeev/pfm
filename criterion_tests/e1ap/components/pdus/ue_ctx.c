#include <rte_hash.h>
#include <rte_jhash.h>
#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include "pdus.h"
#include "drb.h"
#include <string.h>

#define PFM_UE_CTX_TABLE_NAME "UE_CTX_TABLE_HASH"
#define PFM_UE_IDS_TABLE_NAME "UE_IDS_TABLE_HASH"

static ue_ctx_t ue_ctx_table_g[MAX_UE_COUNT];
static uint32_t last_allocated_slot_g = 0;
static struct rte_hash* ue_ctx_hashtable_g;
static pfm_bool_t ue_ctx_table_up_g = PFM_FALSE;

static uint32_t last_assigned_id_g = 0;


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
	ue_ctx_table_up_g = PFM_TRUE;
	pfm_trace_msg("ue_ctx_table initialized");
	return PFM_SUCCESS;
}

pfm_retval_t
ue_ctx_id_alloc(uint32_t *ue_id)
{
	pfm_retval_t ret_val;
	uint32_t i;
	int ret;
	if (ue_ctx_table_up_g != PFM_TRUE)
	{
		ret_val = ue_ctx_table_init();
		if (ret_val != PFM_FAILED)	
		{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Failed to init ue_ctx_table ");
			return PFM_FAILED;
		}
		pfm_trace_msg("Initialised ue_ctx_table");
	}

	for (i = (last_assigned_id_g + 1)%MAX_UE_COUNT;i != last_assigned_id_g;
		i = (i+1)%MAX_UE_COUNT)
	{
		ret = rte_hash_lookup(ue_ctx_hashtable_g,&i);
		if (ret == -ENOENT)
		{
			pfm_trace_msg("Assigning ue id :: %d",i);
			last_assigned_id_g = i;
			*ue_id = i;
			return PFM_SUCCESS;
		}

		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ALERT,"Error in rte_hash_lookup()");
			return PFM_FAILED;
		}
	}
	pfm_log_msg(PFM_LOG_ERR,"no free ue id available");
	return PFM_FAILED;
}

static ue_ctx_t*
ue_ctx_alloc(uint32_t ue_id)
{
	uint32_t i;
	for (i = (last_allocated_slot_g +1)%MAX_UE_COUNT;i != last_allocated_slot_g;
		i = (i+1)%MAX_UE_COUNT)  
	{	
		if (ue_ctx_table_g[i].is_row_used != PFM_TRUE)
		{
			ue_ctx_table_g[i].cuup_ue_id 	= ue_id;
			ue_ctx_table_g[i].is_row_used 	= PFM_TRUE;
			last_allocated_slot_g = i;
			return  &(ue_ctx_table_g[i]);
		}
	}
	pfm_log_msg(PFM_LOG_ALERT,"ue_ctx_table is full");
	return NULL;
}




ue_ctx_t *
ue_ctx_add(uint32_t ue_id)
{
	int ret;
	ue_ctx_t *entry;
	if (ue_ctx_table_up_g == PFM_FALSE)	
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
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,(void **)&entry);
	if (ret == 0)
	{
		pfm_log_msg(PFM_LOG_ERR,"attempt to add existing entry");
		return NULL;
	}
	entry = ue_ctx_alloc(ue_id);
	// Circular queue style search for empty entry 
	if (entry ==  NULL)
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table is full");
	return entry;
}

pfm_retval_t	
ue_ctx_remove(uint32_t ue_id)
{
	int ret,i;	
	ue_ctx_t * entry;
	if (ue_ctx_table_up_g == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"invoking ue_ctx_remove() before init");
		return PFM_FAILED;
	}
	
	// Get the tunnel entry
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,(void **)&entry);
	// Handle if not present 
	if (ret < 0)	
	{
		if ( ret ==  -ENOENT)  
		{
			pfm_log_msg(PFM_LOG_ERR,"entry :: %d not in ue_ctx_table",
						ue_id);
			return PFM_NOT_FOUND;
		}
		pfm_log_msg(PFM_LOG_ERR,"rte_hash_lookup_data() for id :: %d failed",
					ue_id);
		return PFM_FAILED;	
	}

	entry->is_row_used = PFM_FALSE;
	
	// Clear tunnel lists
	for (i = 0;i < entry->pdus_count ; i++)
		 pdus_remove(&((entry->pdus_tunnel_list[i])->key));
	for (i = 0;i < entry->drb_count ; i++)
		 drb_remove(&((entry->drb_tunnel_list[i])->key));
	return PFM_SUCCESS;
}

ue_ctx_t *	
ue_ctx_modify(uint32_t ue_id)
{
	int ret;	
	ue_ctx_t *entry,*new_ctx;
	if (ue_ctx_table_up_g == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"invoked ue_ctx_modify() before ue_ctx_table_init()");
		return NULL;
	}
	
	// Check if an instance with this ue_id already exists
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,(void **)&entry);
	// If it doesn't exist it is an invalid request
	if (ret < 0)	
	{
		if (ret == -ENOENT)
			pfm_log_msg(PFM_LOG_ERR,"Attempt to modify %d that doesn't exist",
						ue_id);
		else
			pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return NULL;

	}
	
	// Circular queue style search for empty entry 
	new_ctx = ue_ctx_alloc(ue_id);
	if (new_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table full");
		return NULL;
	}


	// Copy the pdus_list of the old entry to the new one so we don't lose track
	for (new_ctx->pdus_count = 0;
		new_ctx->pdus_count < entry->pdus_count;
		new_ctx->pdus_count++)
	{
		new_ctx->pdus_tunnel_list[new_ctx->pdus_count] 
		= entry->pdus_tunnel_list[new_ctx->pdus_count];
	}


	// Copy the drb_list as well
	for (new_ctx->drb_count =0;
		new_ctx->drb_count < entry->drb_count;
		new_ctx->drb_count++)
	{
		new_ctx->drb_tunnel_list[new_ctx->drb_count] 
		= entry->drb_tunnel_list[new_ctx->drb_count];
	}
	return new_ctx;
}

pfm_retval_t	
ue_ctx_commit(ue_ctx_t *new_ctx)
{
	int ret;	
	pfm_retval_t ret_val;
	uint32_t i,j;
	ue_ctx_t* entry;
	tunnel_t* tunnel_entry;
	if (ue_ctx_table_up_g == PFM_FALSE)	
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
		ret_val		= drb_commit(tunnel_entry);
	// If the tunnel_remove() was called on tunnel don't include in drb_tunnel_list
		if (tunnel_entry->is_row_used != PFM_TRUE)
			new_ctx->drb_tunnel_list[i] = NULL;
	}

	// Pack the drb_tunnel_list removing the ones assigned as NULL
	for (i = 0,j = 0;i < new_ctx->drb_count;i++)
	{
		if (new_ctx->drb_tunnel_list[i] !=  NULL)
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
		ret_val		= pdus_commit(tunnel_entry);

		if (tunnel_entry->is_row_used != PFM_TRUE)
			new_ctx->pdus_tunnel_list[i] = NULL;
	}

	// Pack the pdus_tunnel_list
	for (i = 0,j = 0;i < new_ctx->pdus_count;i++)
	{
		if (new_ctx->drb_tunnel_list[i] !=  NULL)
		{
			new_ctx->pdus_tunnel_list[j] = new_ctx->drb_tunnel_list[i];
			j++;
		}
	}

	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,
				   &(new_ctx->cuup_ue_id),
				   (void **)&entry);
	// if we don't find an entry with new_ctx's cuup_ue_id
	if (ret < 0)
	{
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
		pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
		return PFM_FAILED;
	}
	
	else 
	{
		// If the entry found in table is same as the one we're about to commit
		if (entry ==  new_ctx)
		{
			// If it had ue_ctx_remove called on it is_row_used will be false
			if (entry->is_row_used != PFM_TRUE)
			{
				//delete the key from ue_ctx_hashtable_g	
				ret = rte_hash_del_key(ue_ctx_hashtable_g,&(new_ctx->cuup_ue_id));
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
	if (ue_ctx_table_up_g == PFM_FALSE)	
	{
		pfm_log_msg(PFM_LOG_ERR,"ue_ctx_table uninitialize");
		return NULL;
	}
	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&ue_id,(void **)&entry);
	if (ret == 0)	
		return entry;
	if (ret < 0)	
	{
		if (ret == -ENOENT)	
			pfm_trace_msg("No entry found in ue_ctx_table id :: %d",
					ue_id);
		if (ret == -EINVAL)	
			pfm_log_msg(PFM_LOG_ERR,"Invalid parameters %d in ue_ctx_get",
						ue_id);
		pfm_log_rte_err(PFM_LOG_ERR,"rte_hash_lookup_data() failed");
	}	
	return NULL;
}

pfm_retval_t
ue_ctx_rollback(ue_ctx_t *ue_ctx)
{
	int ret;
	uint32_t i;
	pfm_retval_t ret_val;
	ue_ctx_t *entry;

	if (ue_ctx_table_up_g != PFM_TRUE)
	{
		pfm_log_msg(PFM_LOG_ERR,"UE CTX table uninitialized");
		return PFM_FAILED;
	}
	
	// Check if there is an entry that is already committed

	ret = rte_hash_lookup_data(ue_ctx_hashtable_g,&(ue_ctx->cuup_ue_id),(void **)&entry);
	// If there is no already committed entry
	if (ret < 0)
	{
		if (ret ==  -ENOENT)
		{
			// If there is no entry we got this from ue_ctx add
			// rollback each of the tunnels
			// Mark the ue_ctx as not used
			if (ue_ctx->is_row_used == PFM_TRUE)
			{
				for (i = 0;i < ue_ctx->pdus_count;i++)
					pdus_rollback(ue_ctx->pdus_tunnel_list[i]);
				for (i = 0;i < ue_ctx->drb_count;i++)
					drb_rollback(ue_ctx->drb_tunnel_list[i]);
				ue_ctx->is_row_used = PFM_FALSE;
				return PFM_SUCCESS;
			}
			pfm_log_msg(PFM_LOG_ERR,"Invalid state");
			return PFM_FAILED;
		}

		else if (ret == -EINVAL)
			pfm_log_msg(PFM_LOG_ERR,"Error in rte_hash_lookup_data()");
		return PFM_FAILED;
	}
	// If there is a committed entry it the ue_ctx could be from 
	// ue_ctx_remove
	// ue_ctx_modify
	// or simply and unmodified ue_ctx

	// If the entry was removed or unmodified
	if (ue_ctx == entry)
	{
		// If the entry was removed rollback the tunnels 
		if (entry->is_row_used ==  PFM_FALSE)
		{
			for (i = 0;i < ue_ctx->pdus_count;i++)
				ret_val = pdus_rollback(ue_ctx->pdus_tunnel_list[i]);
			for (i = 0;i < ue_ctx->drb_count;i++)
				ret_val =drb_rollback(ue_ctx->drb_tunnel_list[i]);
			entry->is_row_used =  PFM_TRUE;
		}
		return PFM_SUCCESS;
	}

	// If nt and entry are not equal to each other and have is_row_used 
	// PFM_TRUE then obtained form ue_ctx_modify

	if ((ue_ctx->is_row_used == PFM_TRUE) && (entry->is_row_used == PFM_TRUE))
	{
		// if entry was modified rollback the tunnels in the old entry
		for (i = 0;i < entry->pdus_count;i++)
			ret_val = pdus_rollback(entry->pdus_tunnel_list[i]);
		for (i = 0;i < entry->drb_count;i++)
			ret_val = drb_rollback(entry->drb_tunnel_list[i]);
		
		ue_ctx->is_row_used = PFM_FALSE;
		return PFM_SUCCESS;
	}
	return PFM_FAILED;
}


void	
ue_ctx_print_list(FILE *fp)
{	
	const uint32_t *key_ptr;
	ue_ctx_t * data_ptr;
	uint32_t ptr = 0,hash_count;
	int ret;
	if (ue_ctx_table_up_g == PFM_FALSE)	
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


