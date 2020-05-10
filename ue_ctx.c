#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"

static ue_ctx_t ue_ctx_table_g[MAX_UE_COUNT];
static uint32_t last_allocated_slot_g = 0;

ue_ctx_t *	ue_ctx_add(uint32_t ue_id)
{
	printf("ue_ctx_add(ueId=%d) invoked. "
		"But not implemented\n",
		ue_id);
	printf("table=%p,last_slot=%d\n",
		ue_ctx_table_g,last_allocated_slot_g);

/*
1. get new free slot idex by serch ue_ctx_table_g[] with
   'is_used' set to false. start from last_allocated_slot_g
   wrap around when reaching MAX_UE_COUNT
2. set 'is_used' to true;
3. return the new row addr
*/
	return NULL;
}

pfm_retval_t	ue_ctx_remove(uint32_t ue_id)
{
	printf("ue_ctx_remove(ueId=%d) invoked. "
		"But not implemented\n",
		ue_id);

/* 
1. use hash table to get record in ue_ctx_table_g for the UE.
2. delete enty from hash table so that it is not availanle for
   any consumers
3. invoke tunnel_remove() each tunnel in pdus_tunnel_list[]
4. invoke tunnel_remove() each tunnel in drb_tunnel_list[]
5. make the row of ue_ctx_table_g as free.
*/
	return PFM_SUCCESS;
}

ue_ctx_t *	ue_ctx_modify(uint32_t ue_id)
{
	printf("ue_ctx_modify(ueId=%d) invoked. "
		"But not implemented\n",
		ue_id);
	return NULL;
}

pfm_retval_t	ue_ctx_commit(ue_ctx_t *new_ctx)
{
	printf("ue_ctx_commit(ctx=%p) invoked. "
		"But not implemented\n",
		new_ctx);
	return PFM_SUCCESS;
}

ue_ctx_t *	ue_ctx_get(uint32_t ue_id)
{
	printf("ue_ctx_get(ueId=%d) invoked. "
		"But not implemented\n",
		ue_id);
	return NULL;
}

void		ue_ctx_print_list(FILE *fp)
{
	printf("ue_ctx_print_list(fp=%p) invoked. "
		"But not implemented\n",
		fp);
	return;
}

void		ue_ctx_print_show(FILE *fp, uint32_t ue_id)
{
	printf("ue_ctx_print_show(fp=%p,ueId=%d) invoked. "
		"But not implemented\n",
		fp,ue_id);
	return;
}


