#include "pfm.h"
#include "cuup.h"
#include "e1ap_bearer_modify.h"
#include "e1ap_comm.h"
#include "ue_ctx.h"
#include "tunnel.h"



/*e1ap_bearer_modify is used to make any changes to an already exiting ue context
 The changes can be of three types 
	- add a new pdus session for the ue
	- remove an existing pdus_session 
	- make changes to an existing pdus_session 
*/
static pfm_retval_t
bearer_modify_failure(e1ap_bearer_ctx_modify_req_t *req,
		      e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cuup_ue_id = req->cuup_ue_id;
	
	rsp->pdus_setup_succ_count  = 0;
	rsp->pdus_modify_succ_count = 0;
	
	rsp->pdus_setup_fail_count  = req->pdus_setup_count;
	rsp->pdus_modify_fail_count = req->pdus_modify_count;

	memset(pdus_setup_succ_list,
		0,
		sizeof(pdus_setup_succ_count)*MAX_PDUS_PER_UE);
	memset(pdus_modify_succ_list,
		0,
		sizeof(pdus_modify_succ_count)*MAX_PDUS_PER_UE);

	for (int i = 0;int i < pdus_setup_count;i++)	
	{
		rsp->drb_setup_fail_list[i].pdus_id = req->pdus_setup_list[i].pdus_id;
		rsp->drb_setup_fail_list[i].cause   = 0
	}
	
	for (int i = 0;int i < pdus_modify_count;i++)	
	{
		rsp->drb_modify_fail_list[i].pdus_id = req->pdus_modify_list[i].pdus_id;
		rsp->drb_modify_fail_list[i].cause   = 0;
	}
	return PFM_SUCCESS;
}


static tunnel_t*
bearer_pdus_setup(tunnel_key_t *tunnel_key,pdus_setup_req_info_t *req)
{
	pfm_retval_t ret;
	tunnel_t* tunnel_entry = tunnel_add(tunnel_key);
	if (tunnel_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Error allocating tunnel entry");
		return NULL;
	}
	tunnel_entry->remote_ip 	= req->pdus_ul_ip_addr;
	tunnel_entry->remote_te_id 	= req->pdus_ul_teid;
	tunnel_entry->tunnel_type	= TUNNEL_TYPE_PDUS;
	tunnel_entry->pdus_info.pdus_id = req->pdus_id;
	ret = tunnel_commit(tunnel_entry);
	
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,
				"Error committing tunnel entry");
		return NULL;
	}
	return tunnel_entry;
}

static void
bearer_pdus_setup_succes(tunnel_t *tunnel_entry,
		           pdus_setup_succ_rsp_info_t* pdus_succ_rsp)
{
	pdus_succ_rsp->pdus_id 			 = tunnel_entry->pdus_info.pdus_id;
	pdus_succ_rsp->drb_setup_succ_count	 = 0;
	pdus_succ_rsp->drb_setup_fail_count	 = 0;
	pdus_succ_rsp->pdus_dl_ip_addr		 = tunnel_entry->key.ip_addr;
	pdus_succ_rsp->pdus_dl_teid		 = tunnel_entry->key.te_id;
	return;
}

static void
bearer_pdus_setup_failed(pdus_setup_req_info_t *req,
			 pdus_setup_fail_rsp_info_t* pdus_fail_rsp)
{
	pdus_fail_rsp->pdus_id = req->pdus_id;
	// TD find a way to assign cause
	pdus_fail_rsp->cause   = 1;
	return;
}

static tunnel_t*
bearer_drb_setup(tunnel_key_t *tunnel_key,drb_setup_req_info_t *req)
{
	pfm_retval_t ret;
	tunnel_t* tunnel_entry;
	tunnel_entry = tunnel_add(tunnel_key);
	if (tunnel_entry == NULL){
		pfm_log_msg(PFM_LOG_ERR,
			    "Error allocating tunnel entry");
		return NULL;
	}

	tunnel_entry->remote_ip_addr	 = req->drb_dl_ip_addr;
	tunnel_entry->remote_te_id   	 = req->drb_dl_teid;
	tunnel_entry->drb_info.drb_id    = req->drb_id;
	
	ret = tunnel_commit(tunnel_entry);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error committing tunnel entry");
		return NULL
	}
	return tunnel_entry;
}

static void
bearer_drb_setup_success(tunnel_t* tunnel_entry,drb_setup_succ_rsp_info_t *drb_succ_rsp)
{
	drb_succ_rsp->drb_id		 = tunnel_entry->drb_info.drb_id;
	drb_succ_rsp->drb_ul_ip_addr 	 = tunnel_entry->key.ip_addr;
	drb_succ_rsp->drb_ul_teid	 = tunnel_entry->key.te_id;
	return;
}

static void 
bearer_drb_setup_failed(drb_setup_req_info_t *req,
		        drb_setup_fail_rsp_info_t *drb_fail_rsp)
{
	drb_fail_rsp->drb_id = req->drb_id
	//TD find way to assign cause
	drb_fail_rsp->cause = 1;
	return;
}

pfm_retval_t e1ap_bearer_ctx_modify(e1ap_bearer_ctx_modify_req_t *req,
			            e1ap_bearer_ctx_modify_rsp_t *rsp)
{
	pfm_retval_t ret;
	ue_ctx_t * ue_ctx;
	tunnel_key_t *tunnel_key;
	tunnel_t* tunnel_entry;
	// Error if request is invalid 
	if (req == NULL )	
	{
		if (rsp != NULL)
			bearer_modify_failure(req,rsp);
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid request");
		return PFM_FAILED;
	}
	ue_ctx = ue_ctx_modify(req->cuup_ue_id);
	// Error if ue_ctx doesn't exist for given cuup_ue_id	
	if (ue_ctx == NULL)
	{
		if (rsp != NULL)
			bearer_modify_failure(req,rsp);
		pfm_log_msg(PFM_LOG_ERR,
			"ue_ctx doesn't exist for req ue_id");
		return PFM_FAILED;
	}
	// setting up the rsp
	rsp->cucp_ue_id = req->cucp_ue_id;
	rsp->cuup_ue_id = req->cuup_ue_id;
	// TD do something with the cause parameter
	rsp->cause	= cause;
	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = 0;
	
	rsp->pdus_modify_succ_count = 0;
	rsp->pdus_modify_fail_count = 0;
	// servicing setup requests
	for (int i = 0;i < req->pdus_setup_count;i++)
	{
		// Setting up pointers to frequently used but really long named values	
		pdus_setup_fail_rsp_info_t *pdus_setup_fail = 
		&(rsp->pdus_setup_fail_list[rsp->pdus_setup_fail_count]);

		pdus_setup_succ_rsp_info_t *pdus_setup_succ = 
		&(rsp->pdus_setup_succ_list[rsp->pdus_setup_succ_count]);	

		ret = tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_PDUS);
		if (ret == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,
					"Error allocating tunnel key");
			bearer_pdus_setup_failed(&(req->pdus_list[i]),pdus_setup_fail);
			rsp->pdus_setup_fail_count++;
			continue;
		}
		tunnel_entry = bearer_pdus_setup(tunnel_key,&(req->pdus_list[i]));

		if (tunnel_entry == NULL)
		{
			pfm_log_msg(PFM_LOG_ERR,
				    "pdus_create failed");
			//TD pdus failure handle
			bearer_pdus_setup_failed(&(req->pdus_list[i]),pdus_setup_fail);
			rsp->pdus_setup_fail_count++;
			continue;
		}
	
		// TD flow_count,ul_new_flow_detected_bit_map,flow_list
		ue_ctx->pdus_list[ue_ctx->pdus_count] = tunnel_entry;

		for (int j = 0; j < req->pdus_setup_list[i].drb_count;j++)
		{
			// Setting up pointers to frequently used but really wrong names
			drb_setup_succ_rsp_info_t *drb_setup_succ = 
			pdus_setup_succ->drb_succ_list[pdus_setup_succ->drb_setup_succ_count];
			
			drb_setup_fail_rsp_info_t *drb_setup_fail = 
			pdus_setup_fail->drb_fail_list[pdus_setup_succ->drb_setup_fail_count];
	
			drb_setup_req_info_t* drb_req = 
			req->pdus_setup_list[i].drb_list[j];	
			ret = tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_DRB);
			if (ret == PFM_FAILED)
			{
				pfm_log_msg(PFM_LOG_ERR,
					"Error allocating tunnel key");
				bearer_drb_setup_failed(drb_req,drb_setup_fail);
				pdus_setup_succ->drb_setup_fail_count++;
				continue;
			}
			
			tunnel_entry = bearer_drb_setup(tunnel_entry,drb_req);
	
			if (tunnel_entry == NULL)
			{
				pfm_log_msg(PFM_LOG_ERR,
					    "drb_setup failed");
				bearer_drb_setup_failed(drb_req,drb_setup_fail)
				pdus_setup_succ->drb_setup_fail_count++;
				continue;
			}
			bearer_drb_setup_success(tunnel_entry,drb_setup_succ);
			pdus_setup_succ->drb_setup_succ_count++;
		}
		rsp->pdus_setup_succ_count++;
	}
	
	// servicing modify requests

	for (int i = 0;i < req->pdus_modify_count;i++)
	{
		

	}

	

}
