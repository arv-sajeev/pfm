#ifndef __DRB_H__
#define __DRB_H__


#include "tunnel.h"
#include "ue_ctx.h"
#include "e1ap_comm.h"
#include "cuup.h"
#include "pfm_comm.h"
#include "pfm.h"
#include "cuup.h"

/*********************

	drb_create
	
	Create a new drb entry with given drb_setup_req_info and create it's 
	drb_setup_succ_rsp_info if required. The entry if created successfully is
	assigned to the ue_ctx but not committed.If entry creation failed creation of 
	failure response must be handled by caller.

	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the drb_setup_req_info to create drb_entry
	rsp		-	pointer to the drb_setup_succ_rsp_info to populate
				if the drb creation was successful

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if drb_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if drb_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
***********************/
pfm_retval_t 
drb_create(ue_ctx_t* ue_ctx,drb_setup_req_info_t* req,drb_setup_succ_rsp_info_t* rsp);

/**********************

	drb_fail_rsp_create

	Create drb_setup_fail_rsp_info if creation of tunnel entry was unsuccessful
	
	@params 
	 
	req		-	pointer to drb_setup_req_info that failed drb_create
	succ_rsp	-	pointer to drb_setup_fail_rsp_info that corresponds to 
				req
	
	@returns
	void

**********************/

void 
drb_fail_rsp_create(drb_setup_req_info_t* req,drb_setup_fail_rsp_info_t *rsp);

#endif
