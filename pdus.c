#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_log.h"
#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include "pdus.h"
#include "drb.h"
#include "e1ap_comm.h"
#include "e1ap_bearer_setup.h"
#include "e1ap_bearer_modify.h"

static void  
pdus_setup_succ_rsp_create(tunnel_t *tunnel_entry,pdus_setup_succ_rsp_info_t *succ_rsp)
{
	succ_rsp->pdus_id 			= tunnel_entry->pdus_info.pdus_id;
	succ_rsp->drb_setup_succ_count 	= 0;
	succ_rsp->drb_setup_fail_count	= 0;
	succ_rsp->pdus_dl_ip_addr		= tunnel_entry->key.ip_addr;
	succ_rsp->pdus_dl_teid		= tunnel_entry->key.te_id;
	return;
}

void
pdus_setup_fail_rsp_create(pdus_setup_req_info_t* req,
			   pdus_setup_fail_rsp_info_t* fail_rsp,
			   e1ap_fail_cause_t cause)
{
	fail_rsp->pdus_id = req->pdus_id;
	// TD how to assign cause
	fail_rsp->cause   = cause;
	return;
}


pfm_retval_t 
pdus_setup(ue_ctx_t* ue_ctx,
	   pdus_setup_req_info_t* req,
	   pdus_setup_succ_rsp_info_t *succ_rsp,
	   pdus_setup_fail_rsp_info_t *fail_rsp)
{
	pfm_retval_t ret;
	tunnel_key_t tunnel_key;
	tunnel_t *tunnel_entry;

	drb_setup_succ_rsp_info_t* drb_succ_rsp;
	drb_setup_fail_rsp_info_t* drb_fail_rsp;
	
	// Assign a tunnel key with pdus_ul_ip_addr
	ret = tunnel_key_alloc(req->pdus_ul_ip_addr,TUNNEL_TYPE_PDUS,&tunnel_key);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_key");
		// TD assign cause properly
		pdus_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}

	// Allocate a free entry from the tunnel table 
	
	tunnel_entry = tunnel_add(&tunnel_key);
	if (tunnel_entry ==  NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_entry");
		// TD assign cause properly
		tunnel_key_free(&tunnel_key);
		pdus_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}
	
	// Fill in the pdus details to the entry
	tunnel_entry->remote_ip    	= req->pdus_ul_ip_addr;
	tunnel_entry->remote_te_id 	= req->pdus_ul_teid;
	tunnel_entry->tunnel_type 	= TUNNEL_TYPE_PDUS;
	tunnel_entry->pdus_info.pdus_id = req->pdus_id;
	
	// Setup up the succ_rsp since it's gone fine so far
	// Rest of the info is added as the drb's are processed

	pdus_setup_succ_rsp_create(tunnel_entry,succ_rsp);
	
	for (int i = 0;i < req->drb_count;i++)	{
		// Storing frequently used pointers to make the variable names smaller
		drb_succ_rsp = &(succ_rsp->drb_succ_list[succ_rsp->drb_setup_succ_count]);
		drb_fail_rsp = &(succ_rsp->drb_fail_list[succ_rsp->drb_setup_fail_count]);

		// Process this drb
		ret = drb_setup(ue_ctx,&(req->drb_list[i]),drb_succ_rsp,drb_fail_rsp);
		
		// If failed log and increment the drb_setup_fail_count
		if (ret ==  PFM_FAILED){
			pfm_log_msg(PFM_LOG_ERR,"Error creating DRB entry");
			succ_rsp->drb_setup_fail_count++;
			continue;
		}
		succ_rsp->drb_setup_succ_count++;
	}
	// If all drb requests fail consider the pdu request a fail
	if (succ_rsp->drb_setup_fail_count == req->drb_count)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"all drb_req in pdus_req failed pdus id :: %s:",
				req->pdus_id);
		tunnel_remove(&tunnel_key);
		tunnel_commit(tunnel_entry);
		
		pdus_setup_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
		return PFM_FAILED;
	}

	// Assign tunnel_entry to the next vacant spot
	ue_ctx->pdus_tunnel_list[ue_ctx->pdus_count] = tunnel_entry;
	ue_ctx->pdus_count++;
	return PFM_SUCCESS;
}


//---------------------------------------------------pdus_modify------------------------

static void
pdus_modify_succ_rsp_create(tunnel_t* tunnel_entry,pdus_modify_succ_rsp_info_t* succ_rsp)
{
	succ_rsp->pdus_id 		= tunnel_entry->pdus_info.pdus_id;
	succ_rsp->drb_setup_succ_count 	= 0;
	succ_rsp->drb_setup_fail_count  = 0;
	succ_rsp->drb_modify_succ_count = 0;
	succ_rsp->drb_modify_fail_count = 0;
	succ_rsp->pdus_dl_ip_addr	= tunnel_entry->key.ip_addr;
	succ_rsp->pdus_dl_teid		= tunnel_entry->key.te_id;
	return;
}


void 
pdus_modify_fail_rsp_create(pdus_modify_req_info_t* req,
			    pdus_modify_fail_rsp_info_t* fail_rsp,
			    e1ap_fail_cause_t cause)
{
	fail_rsp->pdus_id = req->pdus_id;
	// TD how to assign cause
	fail_rsp->cause   = cause;
	return;
}

pfm_retval_t
pdus_modify(ue_ctx_t* ue_ctx,
            pdus_modify_req_info_t *req,
	    pdus_modify_succ_rsp_info_t *succ_rsp,
	    pdus_modify_fail_rsp_info_t *fail_rsp,
	    uint32_t idx)
{
	pfm_retval_t ret = PFM_FAILED;
	tunnel_t *pdus_entry,*old_entry;
	drb_setup_succ_rsp_info_t* drb_succ_rsp;
	drb_setup_fail_rsp_info_t* drb_fail_rsp;
	
	// Get the old tunnel entry
	old_entry = ue_ctx->pdus_tunnel_list[idx];
	pdus_entry = tunnel_modify(&(old_entry->key));
	uint32_t i,j;
	if (pdus_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"tunnel_modify() on tunnel that doesn't exist");
		// TD cause
		pdus_modify_fail_rsp_create(req,fail_rsp,FAIL_CAUSE_RNL_UNKNOWN_PDUS_ID);
		return PFM_FAILED;
	}

	// TD Do stuff with the tunnel as required
	pdus_entry->remote_ip    	= old_entry->remote_ip;
	pdus_entry->remote_te_id 	= old_entry->remote_te_id;
	pdus_entry->tunnel_type 	= old_entry->tunnel_type;
	pdus_entry->pdus_info.pdus_id 	= old_entry->pdus_info.pdus_id;

	// setup the rsp since things have gone fine till now 
	pdus_modify_succ_rsp_create(pdus_entry,succ_rsp);

	for (i = 0;i < req->drb_remove_count;i++)
	{
		// search for the drb_id to delete in drb_list
		for (j = 0;j < ue_ctx->drb_count;j++)
		{
			if (ue_ctx->drb_tunnel_list[i]->drb_info.drb_id == 
				req->drb_remove_list[j].drb_id)
			{
				ret = tunnel_remove(&(ue_ctx->drb_tunnel_list[i]->key));
				break;
			}
		}
		if (j == ue_ctx->drb_count )
			pfm_log_msg(PFM_LOG_ERR,"drb_remove_req item not found");
		if (ret ==  PFM_FAILED)
			pfm_log_msg(PFM_LOG_ERR,"tunnel_remove() failed");
	}

	// Service the modify requests

	for (i = 0;i < req->drb_modify_count;i++)
	{
		drb_succ_rsp= &(succ_rsp->drb_modify_succ_list[succ_rsp->drb_setup_succ_count]);
		drb_fail_rsp= &(succ_rsp->drb_modify_fail_list[succ_rsp->drb_setup_fail_count]);
		ret = PFM_SUCCESS;
		for (j = 0;j < ue_ctx->drb_count;j++)
		{
			if (ue_ctx->drb_tunnel_list[j]->drb_info.drb_id == 
				req->drb_modify_list[i].drb_id)
			{
				ret = drb_modify(ue_ctx,
						 &(req->drb_modify_list[i]),
					   	 drb_succ_rsp,
					   	 drb_fail_rsp,
					   	 j);
				break;
			}
		}
		// The entry is not found in drb_tunnel_list	
		if (j == ue_ctx->drb_count)
		{
			pfm_log_msg(PFM_LOG_ERR,"drb_modify_req drb_id not found");	
			// TD cause
			drb_modify_fail_rsp_create(&(req->drb_modify_list[i]),drb_fail_rsp,1);
			succ_rsp->drb_modify_fail_count++;
			continue;
		}
		// Error in drb_modify
		if (ret == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,"Error in drb_modify()");
			succ_rsp->drb_modify_fail_count++;
			continue;
		}
		succ_rsp->drb_setup_succ_count++;
	}
	
	// Service the setup requests

	for (i = 0;i < req->drb_setup_count;i++)
	{
		drb_succ_rsp = 
		&(succ_rsp->drb_setup_succ_list[succ_rsp->drb_setup_succ_count]);
	
		drb_fail_rsp = 
		&(succ_rsp->drb_setup_fail_list[succ_rsp->drb_setup_fail_count]);
		ret = drb_setup(ue_ctx,&(req->drb_setup_list[i]),drb_succ_rsp,drb_fail_rsp);
		
		// If failed log and increment the drb_setup_fail_count
		if (ret ==  PFM_FAILED){
			pfm_log_msg(PFM_LOG_ERR,"Error creating DRB entry");
			succ_rsp->drb_setup_fail_count++;
			continue;
		}
		succ_rsp->drb_setup_succ_count++;
	}
	ue_ctx->pdus_tunnel_list[idx] = pdus_entry;
	return PFM_SUCCESS;
}



