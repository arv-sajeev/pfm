#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_modify.h"
#include "e1ap_comm.h"
#include "ue_ctx.h"
#include "tunnel.h"
#include "pdus.c"
#include "drb.c"



/*e1ap_bearer_modify is used to make any changes to an already exiting ue context
 The changes can be of three types 
	- add a new pdus session for the ue
	- remove an existing pdus_session 
	- make changes to an existing pdus_session 
*/
static void
bearer_modify_failure(e1ap_bearer_ctx_modify_req_t *req
	 	      ,e1ap_bearer_ctx_modify_rsp_t *rsp
		      ,uint32_t cause)
{
	pfm_log_msg(PFM_LOG_ERR,"Error modifying ue_ctx for bearer");
	rsp->cause	= cause;
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cuup_ue_id	= req->cuup_ue_id;

	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = req->pdus_setup_count;
	
	rsp->pdus_modify_succ_count = 0;
	rsp->pdus_modify_fail_count = req->pdus_modify_count;
	
	for (int i = 0;i < req->pdus_setup_count;i++)
	{
		pdus_setup_fail_rsp_create(&(req->pdus_setup_list[i]),&(rsp->pdus_setup_fail_list[i]),1);
	}
	
	for (int i = 0;i < req->pdus_modify_count;i++)
	{
		pdus_modify_fail_rsp_create(&(req->pdus_modify_list[i]),&(rsp->pdus_modify_fail_list[i]),1);
	}
	return;
}

pfm_retval_t 
e1ap_bearer_ctx_modify(e1ap_bearer_ctx_modify_req_t *req,
			            e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	pfm_retval_t ret;
	ue_ctx_t *ue_ctx;
	uint32_t  ue_id,i,j;

	pdus_modify_succ_rsp_info_t *modify_succ_rsp;
	pdus_modify_fail_rsp_info_t *modify_fail_rsp;
	
	pdus_setup_succ_rsp_info_t* setup_succ_rsp;
	pdus_setup_fail_rsp_info_t* setup_fail_rsp;

	ue_ctx = ue_ctx_modify(req->cuup_ue_id);
	// Get a new entry to fill modified ue_ctx to if doesn't already exist fail request
	if (ue_ctx == NULL)
	{	
		pfm_log_msg(PFM_LOG_ERR,"attempt to modify ue that doesn't exist");
		bearer_modify_failure(req,rsp,1);
		return PFM_FAILED;
	}
	
	// Fill ue_ctx level details of response
	rsp->cucp_ue_id			= req->cucp_ue_id;
	rsp->cuup_ue_id			= req->cuup_ue_id;
	rsp->cause			= 0;
	rsp->pdus_setup_succ_count 	= 0;
	rsp->pdus_setup_fail_count	= 0;
	rsp->pdus_modify_succ_count	= 0;
	rsp->pdus_modify_fail_count	= 0;
	// delete modify then setup to minimize confusion 

	// Servicing remove requests
	for (i = 0;i < req->pdus_remove_count;i++)
	{
		// search the pdus_tunnel_list for the pdus_id to delete
		for (j = 0;j < ue_ctx->pdus_count;j++)
		{
			if (ue_ctx->pdus_tunnel_list[j]->pdus_info.pdus_id == 
			    	req->pdus_remove_list[i].pdus_id)
			{
				// remove the tunnel_entry
				ret = tunnel_remove(&(ue_ctx->pdus_tunnel_list[j]->key));
				break;
			}

		}
		
		if (j == ue_ctx->pdus_count || ret == PFM_FAILED)
			pfm_log_msg(PFM_LOG_ERR,"pdus_remove_req item not found");
	}

	// Servicing modify requests
	for (i = 0;i < req->pdus_modify_count;i++)
	{
		// Saving important pointers
		 
		modify_succ_rsp = &(rsp->pdus_modify_succ_list[rsp->pdus_modify_succ_count]);
		modify_fail_rsp	= &(rsp->pdus_modify_fail_list[rsp->pdus_modify_fail_count]);

		// search for tunnel_entry in pdus_list that matches modify_req pdus_id
		for (j = 0;j < ue_ctx->pdus_count;j++)
		{
			if (ue_ctx->pdus_tunnel_list[j]->pdus_info.pdus_id == 
				req->pdus_modify_list[i].pdus_id)
			{
				// modify the pdus and make change in pdus_tunnel_list
				ret = pdus_modify(ue_ctx,
						  &(req->pdus_modify_list[i]),
						  modify_succ_rsp,
						  modify_fail_rsp,
						  j);
				break;
			}
		}
	
		// if ret ==  PFM_FAILED the fail rsp was already made in pdus_modify
		if (ret == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,"Error in pdus_modify()");
			rsp->pdus_modify_fail_count++;
			continue;	
		}

		// if the entry wasn't found setup the fail_rsp
		if (j == ue_ctx->pdus_count)
		{
			pfm_log_msg(PFM_LOG_ERR,"pdus_modify_req not in pdus_list");
			pdus_modify_fail_rsp_create(&(req->pdus_modify_list[i]),modify_fail_rsp,1);
			rsp->pdus_modify_fail_count++;
			continue;	
		}
		rsp->pdus_modify_succ_count++;

	}
	
	// Service the setup requests
	for (i = 0;i < req->pdus_setup_count;i++)
	{
		setup_succ_rsp = &(rsp->pdus_setup_succ_list[rsp->pdus_setup_succ_count]);
		setup_fail_rsp = &(rsp->pdus_setup_fail_list[rsp->pdus_setup_fail_count]);
		
		ret = pdus_setup(ue_ctx,&(req->pdus_setup_list[i]),setup_succ_rsp,setup_fail_rsp);
		// If failed make failed rsp
		if (ret == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,"Error creating PDUS entry");
			rsp->pdus_setup_fail_count++;
			continue;
		}
		rsp->pdus_setup_succ_count++;
	}
	ret = ue_ctx_commit(ue_ctx);
	// Unable to commit ue fail the entire transaction 
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"attempt to modify ue that doesn't exist");
		bearer_modify_failure(req,rsp,1);
		return PFM_FAILED;
	}
	return PFM_SUCCESS;
}


























