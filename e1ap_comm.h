#ifndef __E1AP_COMM_H__
#define __E1AP_COMM_H__ 1

#include "cuup.h"

// BEARER CONTEXT SETUP REQUEST sec 9.2.2.1 of 3GPP TS 38.463
// PDU Session Resource to Setup List 9.3.3.2
// DRB to Setup Item

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4 
// PDU Session Resource to Setup MODIFICATION List 9.3.3.10
// DRB to Setup Item
typedef struct
{
	uint8_t		drb_id;		// DRB ID 9.3.1.16
	// DL UP parameters	9.3.1.13
	// UP Transport Layer Information	9.3.2.1
	pfm_ip_addr_t   drb_dl_ip_addr; // Transport Layer Addr 9.3.2.4
	uint32_t        drb_dl_teid;    // GTP-TEID 9.3.2.3
} drb_setup_req_info_t; 

// BEARER CONTEXT SETUP REQUEST sec 9.2.2.1
// PDU Session Resource to Setup List 9.3.3.2

// BEARER CONTEXT MODIFICATION REQUEST sec 9.2.2.4
// PDU Session Resource to Setup List 9.3.3.10
typedef struct
{
	uint8_t		pdus_id;	// PDU Session ID 9.3.1.21
	uint8_t		drb_count;	// number of DRB the PDU Session
	
	/* NG UL UP Transport Layer Info 9.3.2.1
	   IP Address & TEID of UPF to where UL NGu packet to be send
	   for the PDU Session */
	pfm_ip_addr_t	pdus_ul_ip_addr;// Transport Layer Addr 9.3.2.4
	uint32_t	pdus_ul_teid;	// GTP-TEID 9.3.2.3

	// DRB To Setup List
	drb_setup_req_info_t	drb_list[MAX_DRB_PER_PDUS];
} pdus_setup_req_info_t; 

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
// PDU Session Resource Setup List sec 9.3.3.5 
// DRB Setup List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Setup MODIFICATION List sec 9.3.3.17 
// DRB Setup List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Modify List 9.3.3.19
// DRB Modified List 
typedef struct
{
	uint8_t		drb_id;		// 9.3.1.16

	// UL UP Parameter -> 9.3.1.13
	// UP Transport Layer Info 9.3.2.1
	// IP Addr and TEDI of CUUP to which DU can send UL pkts 
	pfm_ip_addr_t	drb_ul_ip_addr;	// Transport Layer Addr 9.3.2.4
	uint32_t	drb_ul_teid;	// GTP-TEID 9.3.2.3
	// TODO flow success and failed list to be added
} drb_setup_succ_rsp_info_t; 

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
// PDU Session Resource Setup List sec 9.3.3.5 
// DRB Failed List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Setup MODIFICATION List sec 9.3.3.17 
// DRB Failed List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Modify List 9.3.3.19
// DRB Failed to Modify List 
typedef struct
{
	uint8_t			drb_id;		// 9.3.1.16
	e1ap_fail_cause_t	cause;		// 9.3.1.2
} drb_setup_fail_rsp_info_t; 

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
// PDU Session Resource Setup List sec 9.3.3.5 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Setup MODIFICATION List sec 9.3.3.17 
typedef struct
{	
	uint8_t		pdus_id;		// PDU Session ID 9.3.1.21
	uint8_t		drb_setup_succ_count;
	uint8_t		drb_setup_fail_count;

	// NG DL UP Trasport Layer Info 9.3.2.1
	// IP Address and TEID of CUUP to which UPF can send DL pkts
	pfm_ip_addr_t	pdus_dl_ip_addr; 
	uint32_t	pdus_dl_teid;

	// DRB Setup List
	drb_setup_succ_rsp_info_t	drb_succ_list[MAX_DRB_PER_PDUS]; 

	// DRB Failed List
	drb_setup_fail_rsp_info_t	drb_fail_list[MAX_DRB_PER_PDUS]; 
} pdus_setup_succ_rsp_info_t;

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
// PDU Session Resource Failed List sec 9.3.3.6 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Failed List 9.3.3.18
typedef struct
{
	uint8_t		pdus_id;                // PDU Session ID 9.3.1.21
	e1ap_fail_cause_t cause;                  // Cause 9.3.1.2
} pdus_setup_fail_rsp_info_t;

// BEARER CONTEXT SETUP RESPONSE FAILURE RESPONSE. sec 9.3.1.2 of #3GPP TS 38.463
typedef enum
{
	// First 2 bytes signify the class of the error 
	// Last 2 bytes the exact cause
	// Radio network layer cause
	FAIL_CAUSE_RNL_UNSPECIFIED		= 0x00000001,	// Unspecified Radio network layer cause
	FAIL_CAUSE_RNL_UNKNOWN_CUCP_ID 		= 0x00000002,	// Unknown or already allocated gNB-CU-CP UE E1AP ID
	FAIL_CAUSE_RNL_UNKNOWN_CUUP_ID 		= 0x00000003,	// Unknown or already allocated gNB-CU-UP UE E1AP ID
	FAIL_CAUSE_RNL_UNKNOWN_CU_ID_PAIR	= 0x00000004,	// Unknown or inconsistent pair or UE ID's
	FAIL_CAUSE_RNL_MULTIPLE_PDUS_ID		= 0x00000005,	// Multiple PDUS session id instances
	FAIL_CAUSE_RNL_UNKNOWN_PDUS_ID		= 0x00000006,	// Unknown PDUS id
	FAIL_CAUSE_RNL_MULTIPLE DRB_ID		= 0x00000007,	// Multiple DRB session id instances
	FAIL_CAUSE_RNL_UNKNOWN_DRB_ID		= 0x00000008, 	// Unknown DRB id
	FAIL_CAUSE_RNL_PROCEDURE_CANCELLED	= 0x00000009,	// Procedure cancelled
	FAIL_CAUSE_RNL_RESOURCE_UNAVAIL		= 0x0000000A,	// The Radio resource is unavailable
	FAIL_CAUSE_RNL_NORMAL_RELEASE		= 0x0000000B,	// Failure caused due to normal release
	
	// Transport layer cause
	FAIL_CAUSE_TL_UNSPECIFED		= 0x00010000, 	// Unspecified Transport layer cause
	FAIL_CAUSE_TL_RESOURCE_UNAVAIL		= 0x00010001,	// Transport resource unavailable 

	// Protocol cause
	FAIL_CAUSE_P_UNSPECIFIED		= 0x00020000,	// Unspecified Protocol error
	FAIL_CAUSE_P_TRANSFER_SYNTAX_ERROR	= 0x00020001,	// Transfer syntax error 
	FAIL_CAUSE_P_ABSTRACT_SYNTAX_ERROR	= 0x00020002,	// Abstract syntax error 

	// Miscellanous cause

	FAIL_CAUSE_MISC_UNSPECIFIED		= 0x00030000,	// Unspecified Miscellaneous cause 
	FAIL_CAUSE_MISC_CP_OVERLOAD		= 0x00030001,	// Control processing overload
	FAIL_CAUSE_MISC_UE_UNAVAILABLE		= 0x00030002,	// Not enough user plane processing resources
	FAIL_CAUSER_MISC_HW_FAILURE		= 0x00030003,	// Hardware failure
}e1ap_fail_cause_t;

#endif
