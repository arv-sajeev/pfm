#include "pfm.h"
#include "cuup.h"
#include "e1ap_error_indication.h"

pfm_retval_t e1ap_error_indication(
		uint32_t cucp_ue_id,	// set to zero if not included
		uint32_t cuup_ue_id,	// set to zero if not included
		uint32_t cause)
{
	// just print the argument. Implementaion can be done later
	printf("e1ap_error_indication(cucpUeId=%d, cuupUeId=%d,cause=%d\n",
			cucp_ue_id,cuup_ue_id,cause);
	return PFM_SUCCESS;
}

