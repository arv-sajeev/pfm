#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_modify.h"

pfm_retval_t e1ap_bearer_ctx_modify(
		e1ap_bearer_ctx_modify_req_t *req,
		e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	printf("Invoked e1ap_bearer_ctx_modify(req=%p,rsp=%p). "
			"But not implemented\n",req,rsp);
	return PFM_SUCCESS;
}
