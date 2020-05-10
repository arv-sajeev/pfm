#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_setup.h"

pfm_retval_t e1ap_bearer_ctx_setup(
		e1ap_bearer_ctx_setup_req_t *req,
		e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	printf("e1ap_bearer_ctx_setup(req=%p,rsp=%p\n",req,rsp);

/*
1. invoke ue_ctx_add() to get a new record
2. Populate all needed fields of the UE_CTX
3. for each PDUS specified in req, invoke tunnel_add()
3.1 populate all needed fields of the tunnel
4. for each DRB specified in req, invoke tunnel_add()
4.1 populate all needed fields of the tunnel
5. invoke tunnel_commit() for each PDUS and DRB tunnel created
7. invoke ue_ctx_commit()

*/
	return PFM_SUCCESS;

}
