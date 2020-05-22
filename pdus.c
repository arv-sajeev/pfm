#include "tunnel.h"
#include "ue_ctx.h"
#include "e1ap_comm.h"


pfm_retval_t 
pdus_create(ue_ctx_t* ue_ctx,pdus_setup_req_info_t* req,pdus_setup_succ_rsp_info_t *rsp)
{
	pfm_retval_t ret;
	tunnel_key_t* tunnel_key;
	tunnel_t * tunnel_entry;
	
	// Assign a tunnel key with pdus_ul_ip_addr
	ret = tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_PDUS,req);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_key");
		return PFM_FAILED;
	}
	// Allocate a free entry from the tunnel table 
	tunnel_entry = tunnel_add(tunnel_key);
	if (tunnel_entry ==  NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_entry");
		return PFM_FAILED;
	}
	
	// Fill in the pdus details to the entry
	tunnel_entry->remote_ip    	= req->pdus_ul_ip_addr;
	tunnel_entry->remote_te_id 	= req->pdus_ul_teid;
	tunnel_entry->tunnel_type 	= TUNNEL_TYPE_PDUS;
	tunnel_entry->pdus_info.pdus_id = req->pdus_id;
	pdus_succ_rsp_create(tunnel_entry,rsp);
	for (int i = 0;i < drb_count;i++)	{
		// Storing frequently used pointers to make the variable names smaller
		drb_setup_succ_rsp_info_t* drb_succ_rsp = 
			rsp->drb_succ_list[rsp->drb_setup_succ_count];

		drb_setup_fail_rsp_info_t* drb_fail_rsp = 
			rsp->drb_fail_list[rsp->drb_setup_fail_count];
		// Process this drb
		ret = drb_create(ue_ctx,req->drb_list[i],drb_succ_rsp);
		
		// If drb_create was'nt successful make fail_rsp
		if (ret ==  PFM_FAILED){
			pfm_log_msg(PFM_LOG_ERR,"Error creating DRB entry");
			drb_fail_create(req->drb_list[i],drb_fail_rsp);
			rsp->drb_setup_fail_count++;
			continue;
		}
		rsp->drb_setup_succ_count++;
	}
	ue_ctx->pdus_tunnel_list[ue_ctx->pdus_count] = tunnel_entry;
	ue_ctx->pdus_count++;
	return PFM_SUCCESS;
}

static void  
pdus_succ_rsp_create(tunnel_t *tunnel_entry,pdus_setup_succ_rsp_info_t *succ_rsp)
{
	rsp->pdus_id 			= tunnel_entry->pdus_info.pdus_id;
	rsp->drb_setup_succ_count 	= 0;
	rsp->drb_setup_fail_count	= 0;
	rsp->pdus_dl_ip_addr		= tunnel_entry->key.ip_addr;
	rsp->pdus_dl_teid		= tunnel_entry->key.te_id;
	return;
}

void
pdus_fail_rsp_create(pdus_setup_req_info_t* req,pdus_setup_fail_rsp_info_t* rsp)
{
	rsp->pdus_id = req->pdus_id;
	// TD how to assign cause
	rsp->cause   = 1;
	return;
}

