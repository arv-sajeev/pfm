#ifndef __PDUS_H__ 
#define __PDUS_H__

#include "tunnel.h"
#include "ue_ctx.h"
#include "e1ap_comm.h"
#include "cuup.h"
#include "pfm_comm.h"
#include "pfm.h"
#include "cuup.h"

/*********************

	pdus_create
	
	Create a new pdus entry with given pdus_setup_req_info and create it's 
	pdus_setup_succ_rsp_info if required. The entry if created successfully is
	assigned to the ue_ctx but not committed.If entry creation failed creation of 
	failure response must be handled by caller.

	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the pdus_setup_req_info to create pdus_entry
	rsp		-	pointer to the pdus_setup_succ_rsp_info to populate
				if the pdu creation was successful

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if pdus_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if pdus_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
***********************/
pfm_retval_t 
pdus_create(ue_ctx_t* ue_ctx,pdus_setup_req_info_t* req,pdus_setup_succ_rsp_info_t* rsp);

/**********************

	pdus_fail_rsp_create

	Create pdus_setup_fail_rsp_info if creation of tunnel entry was unsuccessful
	
	@params 
	 
	req		-	pointer to pdus_setup_req_info that failed pdus_create
	succ_rsp	-	pointer to pdus_setup_fail_rsp_info that corresponds to 
				req
	
	@returns
	void

**********************/

void 
pdus_fail_rsp_create(pdus_setup_fail_rsp_info_t* req,pdus_setup_fail_rsp_info_t *rsp);

