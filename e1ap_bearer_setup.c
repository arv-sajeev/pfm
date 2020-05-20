#include "pfm.h"
#include "pfm_log.h"
#include "pfm_utils.h"
#include "cuup.h"
#include "e1ap_bearer_setup.h"
#include "e1ap_comm.h"
#include "tunnel.h"
#include "ue_ctx.h"

// TD have to implement scheme to allocate local ue_id right now we are using a counter as id
static uint32_t cuup_ue_id  = 0;		
static uint32_t cu_ngu_id = 0;
static uint32_t cu_f1u_id = 0;

// TD have to implement a scheme to allocate tunnel_key using local ip for NGu and F1u and a tunnel_id for each session 
static pfm_retval_t
tunnel_key_allocate(tunnel_key_t *tun_k,tunnel_type_t tun_t)
{
	if (tun_k == NULL)	
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Invalid tunnel key structure");
		return PFM_FAILED;
	}
	switch (tun_t)
	{
		case TUNNEL_TYPE_PDUS:
			tun_k->ip_addr = pfm_str2ip("192.168.57.200");
			tun_k->te_id   = cu_ngu_id++;
			break;
		case TUNNEL_TYPE_DRB:
			tun_k->ip_addr = pfm_str2ip("192.168.58.202");
			tun_k->te_id   = cu_f1u_id++;
			break;
	}
	return PFM_SUCCESS;
}

static pfm_retval_t
bearer_setup_failure(e1ap_bearer_ctx_setup_req_t *req,
		     e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	// Sending a message that None of the entries succeeded
	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = req->pdus_count;

	memset(rsp->pdus_succ_list,
	       0,
	       sizeof(pdus_setup_succ_rsp_info_t)*MAX_PDUS_PER_UE);
	for (int i = 0;i < req->pdus_count;i++)
	{
		// Fill up the fail list with all pdus_id's from the request
		rsp->pdus_fail_list[i].pdus_id =
		req->pdus_list[i].pdus_id;
		// TD assign cause for failure for each pdus
		rsp->pdus_fail_list[i].cause   = 0;
	}
	pfm_log_msg(PFM_LOG_WARNING,
		    "Bearer setup complete failure sending response");
	return PFM_SUCCESS;

}



pfm_retval_t 
e1ap_bearer_ctx_setup(e1ap_bearer_ctx_setup_req_t *req,
		      e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	// TD handle error s
	int i,j,k,l;
	pfm_retval_t ret;
	tunnel_key_t* tunnel_key;
	tunnel_t*     tunnel_entry;
	
	if (req == NULL)
	{
		pfm_log_msg(PFM_LOG_WARNING,
			    "Invalid request");
		if (rsp != NULL)	
			ret = bearer_setup_failure(req,rsp);
		return PFM_FAILED;
	}	
	
	// Allocate a new ue_ctx with calculated ue_id
	ue_ctx_t* ue_ctx = ue_ctx_add(cuup_ue_id);
	if (ue_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error allocating ue_ctx");
		if (rsp != NULL)
			ret = bearer_setup_failure(req,rsp);
		return PFM_FAILED;
	}
	// Increment id only if allocation was successful 
	cuup_ue_id++;

	// Populate fields on ue_ctx
	ue_ctx->cucp_ue_id   = req->cucp_ue_id;
	ue_ctx->pdus_count   = 0;
	ue_ctx->drb_count    = 0;
	// TD new_ul_flow_detected_count
	ue_ctx->new_ul_flow_detected_count = 0;
	// FIll details of the response that we already have
	rsp->cuup_ue_id       = ue_ctx->cuup_ue_id;
	rsp->cucp_ue_id	      = ue_ctx->cucp_ue_id;
	//TD assigning cause and where to it 
	rsp->cause	      = 0;
	rsp->pdus_setup_succ_count = 0;
	rsp->pdus_setup_fail_count = 0;
	// TD find success count and failure count

	

	for (i = 0;i < req->pdus_count && i < MAX_PDUS_PER_UE;i++)	
	{
		tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_PDUS);
		tunnel_entry 					
		= tunnel_add(tunnel_key);

		tunnel_entry->remote_ip 			
		= req->pdus_list[i].pdus_ul_ip_addr;

		tunnel_entry->remote_te_id 			
		= req->pdus_list[i].pdus_ul_teid;	

		tunnel_entry->tunnel_type			
		= TUNNEL_TYPE_PDUS;

		tunnel_entry->pdus_info.pdus_id 		
		= req->pdus_list[i].pdus_id;
		// TD flow_count
		// TD ul_new_flow_detected_bit_map
		// TD flow_list
		ret = tunnel_commit(tunnel_entry);
		if (ret == PFM_SUCCESS)
		{
			k = ue_ctx->pdus_count;

			ue_ctx->pdus_tunnel_list[k] 
			= tunnel_entry;

			rsp->pdus_succ_list[k].pdus_id 			
			= tunnel_entry->pdus_info.pdus_id;

			rsp->pdus_succ_list[k].drb_setup_succ_count	= 0;
			rsp->pdus_succ_list[k].drb_setup_fail_count 	= 0;

			rsp->pdus_succ_list[k].pdus_dl_ip_addr		
			= tunnel_entry->key.ip_addr;

			rsp->pdus_succ_list[k].pdus_dl_teid		
			= tunnel_entry->key.te_id;

			pfm_trace_msg("PDU entry created");
			ue_ctx->pdus_count++;
			rsp->pdus_setup_succ_count++;
		}
		else 
		{	
			k = rsp->pdus_setup_fail_count;
			rsp->pdus_fail_list[k].pdus_id 			
			= tunnel_entry->pdus_info.pdus_id;
			// TD find how to assign cause
			rsp->pdus_fail_list[k].cause = 0;
			pfm_trace_msg("PDU entry failed");
			rsp->pdus_setup_fail_count++;
			continue;
		}
		
		for (j = 0;j < req->pdus_list[i].drb_count && j < MAX_DRB_PER_PDUS;j++)
		{
			
			tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_DRB);
			
			tunnel_entry 			
			= tunnel_add(tunnel_key);
	
			tunnel_entry->remote_ip		
			= req->pdus_list[i].drb_list[j].drb_dl_ip_addr;
			
			tunnel_entry->remote_te_id	
			= req->pdus_list[i].drb_list[j].drb_dl_teid;
			
			tunnel_entry->tunnel_type	
			= TUNNEL_TYPE_DRB;
			
			tunnel_entry->drb_info.drb_id		
			= req->pdus_list[i].drb_list[j].drb_id;
			
			// TD is_dl_sdap_hdr_enabled
			// TD is_ul_sdap_hdr_enabled
			// TD mapped_flow_idx
			// TD mapped_pdus_idx	
			
			ret = tunnel_commit(tunnel_entry);
			
			if (ret ==  PFM_SUCCESS)
			{
				k = ue_ctx->pdus_count-1;
				l = rsp->pdus_succ_list[k].drb_setup_succ_count;
				
				ue_ctx->drb_tunnel_list[ue_ctx->drb_count] 
				= tunnel_entry;
				
				rsp->pdus_succ_list[k].drb_succ_list[l].drb_id 		
				= tunnel_entry->drb_info.drb_id;
				
				rsp->pdus_succ_list[k].drb_succ_list[l].drb_ul_ip_addr 	
				= tunnel_entry->key.ip_addr;
				
				rsp->pdus_succ_list[k].drb_succ_list[l].drb_ul_teid	
				= tunnel_entry->key.te_id;
				
				rsp->pdus_succ_list[k].drb_setup_succ_count++;
				ue_ctx->drb_count++;
				pfm_trace_msg("DRB entry created");
			}

			else {
				k = ue_ctx->pdus_count-1;
				l = rsp->pdus_succ_list[k].drb_setup_fail_count;
				
				rsp->pdus_succ_list[k].drb_fail_list[l].drb_id 
				= tunnel_entry->drb_info.drb_id;
				
				rsp->pdus_succ_list[k].drb_setup_fail_count++;
				pfm_trace_msg("DRB entry failed");
				continue;
			}
		}
	}
	
	ret = ue_ctx_commit(ue_ctx);
	if (ret == PFM_SUCCESS)
		return PFM_SUCCESS;
	else 
	{
		if (rsp != NULL)
			ret = bearer_setup_failure(req,rsp);
		pfm_log_msg(PFM_LOG_WARNING,
			    "Error will committing ue_ctx");
	}
	return PFM_FAILED;
}
