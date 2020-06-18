#ifndef __DRB_H__
#define __DRB_H__


#include "tunnel.h"
#include "ue_ctx.h"
#include "e1ap_comm.h"
#include "e1ap_bearer_modify.h"
#include "cuup.h"
#include "pfm_comm.h"
#include "pfm.h"
#include "cuup.h"

/*********************

	drb_setup
	
	Create a new drb entry with given drb_setup_req_info and create it's 
	drb_setup_succ_rsp_info if required. The entry if created successfully is
	assigned to the ue_ctx but not committed.If entry creation failed creation of 
	failure response must be handled by caller.

	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the drb_setup_req_info to create drb_entry
	succ_rsp	-	pointer to the drb_setup_succ_rsp_info to populate
				if the drb setup was successful
	fail_rsp	-	pointer to the drb_setup_fail_rsp_info to populate in case
				drb setup failed

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if drb_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if drb_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
***********************/
pfm_retval_t 
drb_setup(ue_ctx_t* ue_ctx,
	  drb_setup_req_info_t* req,	
	  drb_setup_succ_rsp_info_t* succ_rsp,
	  drb_setup_fail_rsp_info_t* fail_rsp);

/**********************

	drb_setup_fail_rsp_create

	Create drb_setup_fail_rsp_info if creation of tunnel entry was unsuccessful
	
	@params 
	 
	req		-	pointer to drb_setup_req_info that failed drb_create
	fail_rsp	-	pointer to drb_setup_fail_rsp_info that corresponds to 
				req
	cause		-	The cause for drb_setup failure
	
	@returns
	void

**********************/

void 
drb_setup_fail_rsp_create(drb_setup_req_info_t* req,
			  drb_setup_fail_rsp_info_t *fail_rsp,
			  e1ap_fail_cause_t cause);

/************************
	drb_modify
	
	To modify an existing pdus entry with given drb_setup_req_info and position in 
	drb_tunnel_list to modify the entry and reassign to list and create it's 
	responses fail or success depending on result, the drb in the requests are
	modified and reassigned but not commited
	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the drb_modify_req_info to create drb_entry
	succ_rsp	-	pointer to the drb_modify_succ_rsp_info to populate
				if the drb modification  was successful
	fail_rsp	-	pointer to the drb_modify_fail_rsp_info to populate if 
				the drb modification was unsuccessful
	id_x		-	the index of the entry to be modified in drb_tunnel_list 

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if drb_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if drb_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
**************************/

pfm_retval_t
drb_modify(ue_ctx_t* ue_ctx,
	    drb_modify_req_info_t *req,
	    drb_setup_succ_rsp_info_t *succ_rsp,
	    drb_setup_fail_rsp_info_t *fail_rsp,
	    uint32_t idx);


/***************************

	drb_modify_fail_rsp_create

	create the failure response in case drb_modify fails filling in the required info
	into the provided drb_modify_fail_rsp_info pointer

	@params

	req		-	pointer to the modify request that failed
	fail_rsp	-	pointer to the fail response
	cause		-	reason for failure

	@returns

	void

****************************/
void
drb_modify_fail_rsp_create(drb_modify_req_info_t *req,
			    drb_setup_fail_rsp_info_t *rsp,
			    e1ap_fail_cause_t cause);

pfm_retval_t drb_remove(tunnel_key_t *tunnel_key);

pfm_retval_t drb_commit(tunnel_t *nt);

pfm_retval_t drb_rollback(tunnel_t *nt);
#endif
