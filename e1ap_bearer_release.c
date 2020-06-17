#include "pfm.h"
#include "pfm_utils.h"
#include "pfm_comm.h"
#include "cuup.h"
#include "e1ap_bearer_release.h"
#include "e1ap_comm.h"
#include "ue_ctx.h"
#include "tunnel.h"

pfm_retval_t e1ap_bearer_ctx_release( e1ap_bearer_ctx_release_cmd_t *req)
{
	pfm_retval_t ret;
	const ue_ctx_t* entry;
	
	ret = ue_ctx_remove(req->cuup_ue_id);
	if (ret != PFM_SUCCESS)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error ue_ctx_remove failed %u",
					req->cuup_ue_id);
		return PFM_FAILED;
	}
	entry = ue_ctx_get(req->cuup_ue_id);
	ret = ue_ctx_commit(entry);
	if (ret != PFM_SUCCESS)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error ue_ctx_commit failed %u",
					req->cuup_ue_id);
		return PFM_FAILED;
	}
	pfm_trace_msg("released UE id - %d",req->cuup_ue_id);
	return PFM_SUCCESS;
}

