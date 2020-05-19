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
	// DL UP paramters	9.3.1.13
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
	
	/* NG UL UP Trasport Layer Info 9.3.2.1
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
	// TODO flow sucess and faild list to be added
} drb_setup_succ_rsp_info_t; 

// BEARER CONTEXT SETUP RESPONSE. sec 9.2.2.2 of 3GPP TS 38.463
// PDU Session Resource Setup List sec 9.3.3.5 
// DRB Failed List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resource Setup MODIFICATION List sec 9.3.3.17 
// DRB Failed List 

// BEARER CONTEXT MODIFICATION RESPONSE. sec 9.2.2.5 
// PDU Session Resorce Modify List 9.3.3.19
// DRB Failed to Modify List 
typedef struct
{
	uint8_t		drb_id;		// 9.3.1.16
	uint32_t	cause;		// 9.3.1.2
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
	uint32_t        cause;                  // Cause 9.3.1.2
} pdus_setup_fail_rsp_info_t;

#endif
