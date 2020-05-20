#ifndef __E1AP_BEARER_MODIFY_H__
#define __E1AP_BEARER_MODIFY_H__ 1

#include "cuup.h"
#include "e1ap_comm.h"

typedef pdus_setup_fail_rsp_info_t pdus_modify_fail_rsp_info_t;

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 of 3GPP TS 38.463
// PDU Session Resource to Modify List 9.3.3.11
// DRB To Modify List
typedef struct 
{
	uint8_t drb_id;
} drb_modify_req_info_t;


// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 of 3GPP TS 38.463
// PDU Session Resource to Modify List 9.3.3.11
// DRB To Remove List
typedef struct 
{
	uint8_t drb_id;
} drb_remove_req_info_t;

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 of 3GPP TS 38.463
// PDU Session Resource to Modify List 9.3.3.11
typedef struct
{
	uint8_t	 pdus_id;
	
	uint8_t	drb_setup_count;
	uint8_t	drb_modify_count;
	uint8_t	drb_remove_count;
	drb_setup_req_info_t        drb_setup_list[MAX_DRB_PER_PDUS];
	drb_modify_req_info_t       drb_modify_list[MAX_DRB_PER_PDUS];
	drb_remove_req_info_t       drb_remove_list[MAX_DRB_PER_PDUS];
} pdus_modify_req_info_t;

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 of 3GPP TS 38.463
// PDU Session Resource to Remove List 9.3.3.12
typedef struct 
{
	uint8_t pdus_id;
	uint32_t cause;
} pdus_remove_req_info_t;


// BEARER CONTEXT MODIFICAION RESPONSE. sec 9.2.2.5 
// PDU Session Resorce Modify List 9.3.3.19
typedef struct
{
	uint8_t	pdus_id;                // PDU Session ID 9.3.1.21
	uint8_t	drb_setup_succ_count;
	uint8_t	drb_setup_fail_count;
	uint8_t	drb_modify_succ_count;
	uint8_t	drb_modify_fail_count;

	// NG DL UP Trasport Layer Info 9.3.2.1
	// IP Address and TEID of CUUP to which UPF can send DL pkts
	pfm_ip_addr_t   pdus_dl_ip_addr;
	uint32_t        pdus_dl_teid;
	
	// DRB Setup List
	drb_setup_succ_rsp_info_t drb_setup_succ_list[MAX_DRB_PER_PDUS];

	// DRB Failed List
	drb_setup_fail_rsp_info_t drb_setup_fail_list[MAX_DRB_PER_PDUS];

	// DRB Modificaion List
	drb_setup_succ_rsp_info_t drb_modify_succ_list[MAX_DRB_PER_PDUS];

	// DRB Failed To Modify List
	drb_setup_fail_rsp_info_t drb_modify_fail_list[MAX_DRB_PER_PDUS];
	
} pdus_modify_succ_rsp_info_t;

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 of 3GPP TS 38.463
typedef struct 
{
	uint32_t	cucp_ue_id;
	uint32_t	cuup_ue_id;
	uint8_t pdus_setup_count;
	uint8_t pdus_modify_count;
	uint8_t pdus_remove_count;

	// PDU Session Resource to Setup List 9.3.3.10
	pdus_setup_req_info_t	pdus_setup_list[MAX_PDUS_PER_UE];

	// PDU Session Resource to Modify List 9.3.3.11
	pdus_modify_req_info_t	pdus_modify_list[MAX_PDUS_PER_UE];

	// PDU Session Resource to Remove List 9.3.3.12
	pdus_remove_req_info_t	pdus_remove_list[MAX_PDUS_PER_UE];

} e1ap_bearer_ctx_modify_req_t;


// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 of 3GPP TS 38.463
// BEARER CONTEXT MODIFICATION FAILURE sec 9.2.2.6 of 3GPP TS 38.463
typedef struct 
{
	uint32_t	cucp_ue_id;
	uint32_t	cuup_ue_id;
	uint32_t	cause;	// set to 0 is sucess
	uint8_t pdus_setup_succ_count;
	uint8_t pdus_setup_fail_count;
	uint8_t pdus_modify_succ_count;
	uint8_t pdus_modify_fail_count;

	// PDU Session Resource Setup List 9.3.3.17
	pdus_setup_succ_rsp_info_t pdus_setup_succ_list[MAX_PDUS_PER_UE];

	// PDU Session Resource Failed List 9.3.3.18
	pdus_setup_fail_rsp_info_t pdus_setup_fail_list[MAX_PDUS_PER_UE];

	// PDU Session Resorce Modify List 9.3.3.19
	pdus_modify_succ_rsp_info_t pdus_modify_succ_list[MAX_PDUS_PER_UE];

	// PDU Session Resource Failed To Modify List 9.3.3.20
	pdus_modify_fail_rsp_info_t pdus_modify_fail_list[MAX_PDUS_PER_UE];

} e1ap_bearer_ctx_modify_rsp_t;


pfm_retval_t e1ap_bearer_ctx_modify(
		e1ap_bearer_ctx_modify_req_t *req,
		e1ap_bearer_ctx_modify_rsp_t *rsp);

#endif
