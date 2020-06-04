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



// Assuming that if things go wrong before even an id is allocated the  
// the cuup_ue_id is set as 0 for the failure response
static void 
e1ap_bearer_setup_fail(uint32_t cucp_ue_id,
		       uint32_t cuup_ue_id,
		       e1ap_fail_cause_t cause,
		       e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	pfm_log_msg(PFM_LOG_ERR,"bearer setup failure");
	rsp->cuup_ue_id = cuup_ue_id;
	rsp->cucp_ue_id = cucp_ue_id;
	rsp->cause	= cause;
	rsp->pdus_setup_succ_count = 0;
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
	char ip_str[STR_IP_ADDR_SIZE];
	// Allocate a ue_id
	ret = ue_ctx_id_alloc(&ue_id);
	if (ret ==  PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating ue_id");
		e1ap_bearer_setup_fail(req->cucp_ue_id,MAX_UE_COUNT,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL,rsp);
		return PFM_FAILED;
	}

	// Allocate an empty ue_ctx entry
	ue_ctx = ue_ctx_add(ue_id);
	if (ue_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating ue_ctx");
		ue_ctx_id_free(&ue_id);
		e1ap_bearer_setup_fail(req->cucp_ue_id,MAX_UE_COUNT,FAIL_CAUSE_RNL_RESOURCE_UNAVAIL,rsp);
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
			pfm_log_msg(PFM_LOG_ERR,"Error creating PDUS entry"
						"pdus_id : %u remote-ip : %s"
						" remote-te-id : %u",
						req->pdus_list[i].pdus_id,
						pfm_ip2str(req->pdus_list[i].pdus_ul_ip_addr,ip_str),
						req->pdus_list[i].pdus_ul_teid);
			rsp->pdus_setup_fail_count++;
			continue;
		}
		rsp->pdus_setup_succ_count++;
	}
	// If all pdus_reqs failed

	if (rsp->pdus_setup_succ_count == 0)
	{
		pfm_log_msg(PFM_LOG_ERR,"all pdus requests failed ue_id %d",
			     ue_id);
		ue_ctx_remove(ue_id);
		ue_ctx_commit(ue_ctx);
		e1ap_bearer_setup_fail(req->cucp_ue_id,ue_ctx->cucp_ue_id,FAIL_CAUSE_RNL_UNSPECIFIED,rsp);
		return PFM_FAILED;
	}
	ret = ue_ctx_commit(ue_ctx);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error committing ue_ctx ue_id %d",
				ue_id);
		ue_ctx_remove(ue_id);
		ue_ctx_commit(ue_ctx);
		e1ap_bearer_setup_fail(req->cucp_ue_id,ue_id,FAIL_CAUSE_RNL_UNSPECIFIED,rsp);
	}
	return ret;	
}
