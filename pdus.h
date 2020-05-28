#ifndef __PDUS_H__ 
#define __PDUS_H__

#include "tunnel.h"
#include "ue_ctx.h"
#include "e1ap_comm.h"
#include "e1ap_bearer_modify.h"
#include "cuup.h"
#include "pfm_comm.h"
#include "pfm.h"
#include "cuup.h"

/*********************

	pdus_setup
	
	Create a new pdus entry with given pdus_setup_req_info and create it's 
	pdus_setup_succ_rsp_info if required. The entry if created successfully is
	assigned to the ue_ctx but not committed. The fail_rsp is also created calling
	pdus_setup_fail_rsp_create if failure occurred within pdus_setup if the failure
	to setup pdus occurred outside pdus_setup fail_rsp should be taken care of manually
	

	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the pdus_setup_req_info to create pdus_entry
	succ_rsp	-	pointer to the pdus_setup_succ_rsp_info to populate
				if the pdu setup was successful
	fail_rsp	-	pointer to the pdus_setup_succ_rsp_info to populate
				if pdu setup was unsuccessful

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if pdus_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if pdus_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
***********************/
pfm_retval_t 
pdus_setup(ue_ctx_t* ue_ctx,
	   pdus_setup_req_info_t* req,
	   pdus_setup_succ_rsp_info_t* succ_rsp,
	   pdus_setup_fail_rsp_info_t* fail_rsp);

/**********************

	pdus_setup_fail_rsp_create

	Create pdus_setup_fail_rsp_info if creation of tunnel entry was unsuccessful
	
	@params 
	 
	req		-	pointer to pdus_setup_req_info that failed pdus_create
	fail_rsp	-	pointer to pdus_setup_fail_rsp_info that corresponds to 
				req
	cause		-	cause that caused failure of pdus_setup
	
	@returns
	void

**********************/

void 
pdus_setup_fail_rsp_create(pdus_setup_req_info_t* req,
			   pdus_setup_fail_rsp_info_t *fail_rsp,
			   uint32_t cause);

/************************
	pdus_modify
	
	To modify an existing pdus entry with given pdus_setup_req_info and positon in 
	pdus_tunnel_list to modify the entry and reassign to list and create it's 
	responses fail or success depending on result, the pdus and its associated drb in the
	request are modified but not committed

	@params 

	ue_ctx		-	pointer to the ue_ctx to populate
	req 		-	pointer to the pdus_modify_req_info to create pdus_entry
	succ_rsp	-	pointer to the pdus_modify_succ_rsp_info to populate
				if the pdus modification  was successful
	fail_rsp	-	pointer to the pdus_modify_fail_rsp_info to populate if 
				the pdus modification was unsuccessful
	id_x		-	the index of the entry to be modified in pdus_tunnel_list 

	@returns
	
	pfm_retval_t	-	PFM_TRUE	- 	if pdus_entry creation was
							successful, rsp will be set

				PFM_FALSE	-	if pdus_entry creation was
							unsuccessful, rsp will not 
							be set and fail_rsp must 
							be set by caller 
**************************/

pfm_retval_t
pdus_modify(ue_ctx_t* ue_ctx,
	    pdus_modify_req_info_t* req,
	    pdus_modify_succ_rsp_info_t *succ_rsp,
	    pdus_modify_fail_rsp_info_t *fail_rsp,
	    uint32_t idx);


/***************************

	pdus_modify_fail_rsp_create

	create the failure response in case pdus_modify fails filling in the required info
	into the provided pdus_modify_fail_rsp_info pointer

	@params

	req		-	pointer to the modify request that failed
	fail_rsp	-	pointer to the fail response
	cause		-	reason for failure

	@returns

	void

****************************/
void
pdus_modify_fail_rsp_create(pdus_modify_req_info_t *req,
			    pdus_modify_fail_rsp_info_t *rsp,
			    uint32_t cause);
#endif
