#include "pfm.h"
#include "pfm_log.h"
#include "cuup.h"
#include "e1ap_bearer_setup.h"
#include "ue_ctx.h"

// TD have to implement scheme to allocate local ue_id right now we are using a counter as id
static ue_local_id = 0;		

pfm_retval_t e1ap_bearer_ctx_setup(
		e1ap_bearer_ctx_setup_req_t *req,
		e1ap_bearer_ctx_setup_rsp_t *rsp)
{

	if (req == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid request");
		return PFM_FAILED;
	}
	
	// Allocate a new ue_ctx with calculated ue_id
	ue_ctx_t* ue_ctx = ue_ctx_add(ue_local_id);
	if (ue_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error allocating ue_ctx");
		return PFM_FAILED;
	}
	// Increment id only if allocation was successful 
	ue_local_id++;

	// Populate fields on ue_ctx

	ue_ctx->cp_ue_id   = req->ue_id;
	ue_ctx->pdus_count = req->pdus_count;
	// TD new_ul_flow_detected_count
	ue_ctx->new_ul_flow_detected_count = 0;
	ue_ctx->drb_count  = 0;
	
// pdus_list[MAX_PDUS_PER_UE]

	for (int i = 0;i < req->pdus_count;i++)	
	{
			
	}
	
	

/*
1. Invoke ue_ctx_add() to get a new record
2. Populate all needed fields of the UE_CTX
3. For each PDUS specified in req, invoke tunnel_add()
3.1 populate all needed fields of the tunnel
4. For each DRB specified in req, invoke tunnel_add()
4.1 populate all needed fields of the tunnel
5. Invoke tunnel_commit() for each PDUS and DRB tunnel created
7. Invoke ue_ctx_commit()

*/
	return PFM_SUCCESS;

}
