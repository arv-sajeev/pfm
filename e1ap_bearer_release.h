#ifndef __E1AP_BEARER_RELESE__
#define __E1AP_BEARER_RELESE__ 1

#include "cuup.h"
#include "e1ap_comm.h"

// BEARER CONTEXT RELEASE COMMAND 9.2.2.9
typedef struct 
{
	uint32_t	cuup_ue_id;
} e1ap_bearer_ctx_release_cmd_t;

pfm_retval_t e1ap_bearer_ctx_release( e1ap_bearer_ctx_release_cmd_t *req);

#endif
