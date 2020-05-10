#ifndef __E1AP_BEARER_SETUP_H__
#define __E1AP_BEARER_SETUP_H__ 1

#include "cuup.h"
#include "e1ap_comm.h"

// BEARER CONTEXT SETUP REQUEST  Sec 9.2.2.1 of 3GPP TS 38.463
typedef struct 
{
	uint32_t	cucp_ue_id;

	// PDU Session Resource To Setup List 9.3.3.2
	uint8_t		pdus_count;
	pdus_setup_req_info_t	pdus_list[MAX_PDUS_PER_UE];
} e1ap_bearer_ctx_setup_req_t;

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
typedef struct 
{
	uint32_t	cucp_ue_id;
	uint32_t	cuup_ue_id;
	uint32_t	cause;	// set to 0 is sucess
	uint8_t pdus_setup_succ_count;
	uint8_t pdus_setup_fail_count;
	pdus_setup_succ_rsp_info_t	pdus_succ_list[MAX_PDUS_PER_UE];
	pdus_setup_fail_rsp_info_t	pdus_fail_list[MAX_PDUS_PER_UE];
} e1ap_bearer_ctx_setup_rsp_t;


pfm_retval_t e1ap_bearer_ctx_setup(
		e1ap_bearer_ctx_setup_req_t *req,
		e1ap_bearer_ctx_setup_rsp_t *rsp);

#endif
