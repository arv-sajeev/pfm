#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_release.h"

pfm_retval_t e1ap_bearer_ctx_release( e1ap_bearer_ctx_release_cmd_t *req)
{
	printf("e1ap_bearer_ctx_release(%p) invoked. "
		"But not implemented\n",req);

	/* get ue_id from 'req'. then invoke ue_ctx_remove() */

	return PFM_SUCCESS;
}

