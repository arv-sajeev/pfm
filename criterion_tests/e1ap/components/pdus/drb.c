#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_log.h"
#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include "drb.h"
#include "e1ap_comm.h"
#include "e1ap_bearer_setup.h"
#include "e1ap_bearer_modify.h"

static void 
drb_setup_succ_rsp_create(tunnel_t* tunnel_entry,drb_setup_succ_rsp_info_t* rsp)
{
	rsp->drb_id		= tunnel_entry->drb_info.drb_id;
	rsp->drb_ul_ip_addr	= tunnel_entry->key.ip_addr;
	rsp->drb_ul_teid	= tunnel_entry->key.te_id;
	return;
}

void 
drb_setup_fail_rsp_create(drb_setup_req_info_t *req,
	            drb_setup_fail_rsp_info_t *rsp,
		    e1ap_fail_cause_t cause)
{
	rsp->drb_id = req->drb_id;
	rsp->cause  = cause;
}



pfm_retval_t
drb_setup(ue_ctx_t* ue_ctx,
	   drb_setup_req_info_t* req,
	   drb_setup_succ_rsp_info_t *succ_rsp,
	   drb_setup_fail_rsp_info_t *fail_rsp)
{
	pfm_retval_t ret;
	tunnel_key_t tunnel_key;
	tunnel_t* tunnel_entry;

	if (ue_ctx->drb_count == MAX_DRB_PER_UE)
	{
		pfm_log_msg(PFM_LOG_ERR,"Overflow MAX_DRB_PER_UE");
		drb_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}
	
	//Assign a tunnel key with drb_dl_ip_addr
	ret  = tunnel_key_alloc(req->drb_dl_ip_addr,TUNNEL_TYPE_DRB,&tunnel_key);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_key");
		// TD assign cause properly
		drb_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}

	//Allocate a free entry from the tunnel table 
	tunnel_entry = tunnel_add(&tunnel_key);
	if (tunnel_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_entry");
		drb_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}

	// Fill the drb details
	tunnel_entry->remote_ip		=	req->drb_dl_ip_addr;
	tunnel_entry->remote_te_id	=	req->drb_dl_teid;
	tunnel_entry->tunnel_type	=	TUNNEL_TYPE_DRB;
	tunnel_entry->drb_info.drb_id  = 	req->drb_id;
	// TD many more field to fill

	drb_setup_succ_rsp_create(tunnel_entry,succ_rsp);
	// Assign new drb_entry to next free slot
	ue_ctx->drb_tunnel_list[ue_ctx->drb_count] = tunnel_entry;
	ue_ctx->drb_count++;
	return PFM_SUCCESS;
}


//----------------------------------modify------------------------------


static void 
drb_modify_succ_rsp_create(tunnel_t* tunnel_entry,drb_setup_succ_rsp_info_t* rsp)
{
	rsp->drb_id		= tunnel_entry->drb_info.drb_id;
	rsp->drb_ul_ip_addr	= tunnel_entry->key.ip_addr;
	rsp->drb_ul_teid	= tunnel_entry->key.te_id;
	return;
}

void 
drb_modify_fail_rsp_create(drb_modify_req_info_t *req,
	                   drb_setup_fail_rsp_info_t *rsp,
		    	   e1ap_fail_cause_t cause)
{
	rsp->drb_id = req->drb_id;
	rsp->cause  = cause;
}

pfm_retval_t
drb_modify(ue_ctx_t* ue_ctx,
	   drb_modify_req_info_t* req,
	   drb_setup_succ_rsp_info_t* succ_rsp,
	   drb_setup_fail_rsp_info_t* fail_rsp,
	   uint32_t idx)
{
	tunnel_t *drb_entry,*old_entry;
	old_entry = ue_ctx->drb_tunnel_list[idx];
	
	drb_entry = tunnel_modify(&(old_entry->key));
	
	if (drb_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_modify on tunnel that doesn't exist");
		// TD cause
		drb_modify_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_UNKNOWN_DRB_ID);
		return PFM_FAILED;
	}

	// TD do stuff to the drb
	memcpy(drb_entry,old_entry,sizeof(tunnel_t));
	
	// Create a drb_modify_succ_rsp
	drb_modify_succ_rsp_create(drb_entry,succ_rsp);
	//XXX TEST MOD
	drb_entry->drb_info.mapped_flow_idx = 1;
	// Assign the new drb to the old one
	ue_ctx->drb_tunnel_list[idx] = drb_entry;
	return PFM_SUCCESS;
}

pfm_retval_t
drb_remove(tunnel_key_t* tunnel_key)
{
	return tunnel_remove(tunnel_key);
}


pfm_retval_t 
drb_commit(tunnel_t* nt)
{
	return tunnel_commit(nt);
}

pfm_retval_t
drb_rollback(tunnel_t *nt)
{
	return tunnel_rollback(nt);
}

