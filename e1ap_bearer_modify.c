#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_modify.h"
#include "e1ap_comm.h"
#include "ue_ctx.h"
#include "tunnel.h"



/*e1ap_bearer_modify is used to make any changes to an already exiting ue context
 The changes can be of three types 
	- add a new pdus session for the ue
	- remove an existing pdus_session 
	- make changes to an existing pdus_session 
*/
static pfm_retval_t
bearer_modify_failure(
		e1ap_bearer_ctx_modify_req_t *req,
		e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cuup_ue_id = req->cuup_ue_id;
	
	rsp->pdus_setup_succ_count  = 0;
	rsp->pdus_modify_succ_count = 0;
	
	rsp->pdus_setup_fail_count  = req->pdus_setup_count;
	rsp->pdus_modify_fail_count = req->pdus_modify_count;

	memset(pdus_setup_succ_list,
		0,
		sizeof(pdus_setup_succ_count)*MAX_PDUS_PER_UE);
	memset(pdus_modify_succ_list,
		0,
		sizeof(pdus_modify_succ_count)*MAX_PDUS_PER_UE);

	for (int i = 0;int i < pdus_setup_count;i++)	
	{
		rsp->drb_setup_fail_list[i].pdus_id = req->pdus_setup_list[i].pdus_id;
		rsp->drb_setup_fail_list[i].cause   = 0
	}
	
	for (int i = 0;int i < pdus_modify_count;i++)	
	{
		rsp->drb_modify_fail_list[i].pdus_id = req->pdus_modify_list[i].pdus_id;
		rsp->drb_modify_fail_list[i].cause   = 0;
	}
	return PFM_SUCCESS;
}



pfm_retval_t e1ap_bearer_ctx_modify(
		e1ap_bearer_ctx_modify_req_t *req,
		e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	int ret;
	ue_ctx_t * ue_ctx;
	// Error if request is invalid 
	if (req == NULL )	
	{
		if (rsp != NULL)
			bearer_modify_failure(req,rsp);
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid request");
		return PFM_FAILED;
	}
	ue_ctx = ue_ctx_modify(req->cuup_ue_id);
	// Error if ue_ctx doesn't exist for given cuup_ue_id	
	if (ue_ctx == NULL)
	{
		if (rsp != NULL)
			bearer_modify_failure(req,rsp);
		pfm_log_msg(PFM_LOG_ERR,
			"ue_ctx doesn't exist for req ue_id");
		return PFM_FAILED;
	}
	// settign up the rsp
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cuup_ue_id = req->cuup_ue_id;
	// TD do something with the cause parameter
	rsp->cause	= cause;
	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = 0;
	
	rsp->pdus_modify_succ_count = 0;
	rsp->pdus_modify_fail_count = 0
	// servicing setup requests
	for (int i = 0;i < req->pdus_setup_count;i++)
	{
		
	}
	

}
