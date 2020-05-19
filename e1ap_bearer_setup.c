#include "pfm.h"
#include "pfm_log.h"
#include "cuup.h"
#include "e1ap_bearer_setup.h"
#include "ue_ctx.h"

// TD have to implement scheme to allocate local ue_id right now we are using a counter as id
static uint32_t cu_ue_id  = 0;		
static uint32_t cu_ngu_id = 0;
static uint32_t cu_fiu_id = 0;

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

pfm_retval_t 
e1ap_bearer_ctx_setup(e1ap_bearer_ctx_setup_req_t *req,
		      e1ap_bearer_ctx_setup_rsp_t *rsp)
{
	// TD handle error s
	int i,j;
	tunnel_key_t tunnel_key;
	tunnel_t     tunnel_entry;

	if (req == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid request");
		return PFM_FAILED;
	}
	
	// Allocate a new ue_ctx with calculated ue_id
	ue_ctx_t* ue_ctx = ue_ctx_add(cu_ue_id);
	if (ue_ctx == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error allocating ue_ctx");
		return PFM_FAILED;
	}
	// Increment id only if allocation was successful 
	ue_local_id++;

	// Populate fields on ue_ctx

	ue_ctx->cp_ue_id   = req->ue_id;
	ue_ctx->pdus_count = req->pdus_count;
	// TD new_ul_flow_detected_count
	ue_ctx->new_ul_flow_detected_count = 0;
	ue_ctx->drb_count  = 0;
	

	for (i = 0;i < req->pdus_count && i < MAX_PDUS_PER_UE;i++)	
	{
		tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_PDUS);
		tunnel_entry 			= tunnel_add(tunnel_key);
		tunnel_entry.tunnel_type	= TUNNEL_TYPE_PDUS;
		tunnel_entry.pdus_id 		= req->pdus_list[i].pdus_id;
		tunnel_entry.remote_ip 		= req->pdus_list[i].pdus_ul_ip_addr;
		tunnel_entry,remote_te_id 	= req->pdus_list[i].pdus_ul_teid;	
		pfm_trace_msg("PDU entry created");
		
		for (j = 0;j < req->pdus_list[i].drb_count && j < MAX_DRB_PER_PDUS;j++)
		{
			tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_DRB);
			tunnel_entry 			= tunnel_add(tunnel_key);
			tunnel_entry.tunnel_type	= TUNNEL_TYPE_DRB;
			tunnel_entry.
			


		}
	}
	
	if (i == MAX_PDUS_PER_UE)
	{
		pfm_log_msg(PFM_LOG_ERR,
			    "ue_ctx pdu_list overflow");
		
	}
	

	
	

/*
1. Invoke ue_ctx_add() to get a new record
2. Populate all needed fields of the UE_CTX
3. For each PDUS specified in req, invoke tunnel_add()
3.1 populate all needed fields of the tunnel
4. For each DRB specified in req, invoke tunnel_add()
4.1 populate all needed fields of the tunnel
5. Invoke tunnel_commit() for each PDUS and DRB tunnel created
7. Invoke ue_ctx_commit()
*/
	return PFM_SUCCESS;
}
