#include "pfm.h"
#include "pfm_log.h"
#include "pfm_utils.h"
#include "cuup.h"
#include "e1ap_bearer_setup.h"
#include "e1ap_comm.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include "pdus.h"
#include "drb.h"




static void 
e1ap_bearer_setup_fail(e1ap_bearer_ctx_setup_req_t *req,
		       e1ap_bearer_ctx_setup_rsp_t *rsp,
		       uint32_t cause)
{
	// Assuming if the entire thing fails we give the cuup_ue_id as 0
	int i;
	pfm_log_msg(PFM_LOG_ERR,"Error creating ue_ctx for bearer");

	rsp->cuup_ue_id = 0;
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cause	= cause;
	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = req->pdus_count;
	
	for (i = 0;i < req->pdus_count;i++)
	{
		// TD assign cause properly
		pdus_setup_fail_rsp_create(&(req->pdus_list[i]),&(rsp->pdus_fail_list[i]),1);
	}
	return;
}


pfm_retval_t 
e1ap_bearer_ctx_setup(e1ap_bearer_ctx_setup_req_t *req,
		      e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	int i;
	pfm_retval_t ret;
	ue_ctx_t *ue_ctx;
	uint32_t  ue_id;
	// Allocate a ue_id
	ue_id = ue_ctx_id_allocate();

	// Allocate an empty ue_ctx entry
	ue_ctx = ue_ctx_add(ue_id);
	if (ue_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating ue_ctx");
		e1ap_bearer_setup_fail(req,rsp,1);
		return PFM_FAILED;
	}
	
	// Fill up the basic ue details
	ue_ctx->cucp_ue_id 			= req->cucp_ue_id;
	ue_ctx->pdus_count 			= 0;
	ue_ctx->drb_count 			= 0;
	ue_ctx->new_ul_flow_detected_count 	= 0;
	
	// Setup the rsp values we currently know
	rsp->cuup_ue_id				= ue_ctx->cuup_ue_id;
	rsp->cucp_ue_id				= ue_ctx->cucp_ue_id;
	// Cause is 0 for success
	rsp->cause				= 0;
	rsp->pdus_setup_succ_count		= 0;
	rsp->pdus_setup_fail_count		= 0;
	

	// Service each pdus_create_request
	for (i = 0;i < req->pdus_count;i++)
	{
		// Storing pointers to make stuff easier 
		pdus_setup_succ_rsp_info_t* succ_rsp = 
			&(rsp->pdus_succ_list[rsp->pdus_setup_succ_count]);
		pdus_setup_fail_rsp_info_t* fail_rsp = 
			&(rsp->pdus_fail_list[rsp->pdus_setup_fail_count]);
	
		// service each pdus request	
		ret = pdus_setup(ue_ctx,&(req->pdus_list[i]),succ_rsp,fail_rsp);
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
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error committing ue_ctx");
		e1ap_bearer_setup_fail(req,rsp,1);
	}
	return ret;	
}
