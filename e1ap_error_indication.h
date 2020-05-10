#ifndef _E1AP_ERROR_INDICATION_H_
#define	_E1AP_ERROR_INDICATION_H_ 1

#include "cuup.h"
#include "e1ap_comm.h"

pfm_retval_t e1ap_error_indication(
		uint32_t cucp_ue_id,	// set to zero if not included
		uint32_t cuup_ue_id,	// set to zero if not included
		uint32_t cause);

#endif
