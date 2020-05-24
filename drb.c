#include "tunnel.h"
#include "ue_ctx.h"
#include "drb.h"
#include "e1ap_comm.h"
#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm.h"
#include "cuup.h"

pfm_retval_t
drb_create(ue_ctx_t* ue_ctx,drb_setup_req_info_t* req,drb_setup_succ_rsp_info_t *rsp)
{
	pfm_retval_t ret;
	tunnel_key_t* tunnel_key;
	tunnel_t* tunnel_entry;
	
	//Assign a tunnel key with drb_dl_ip_addr
	ret = tunnel_key_allocate(tunnel_key,TUNNEL_TYPE_DRB,req);
	if (ret == PFM_FAILED)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_key");
		return PFM_FAILED;
	}

	//Allocate a free entry from the tunnel table 
	tunnel_entry = tunnel_add(tunnel_key);
	if (tunnel_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error allocating tunnel_entry");
		return PFM_FAILED;
	}

	// Fill the drb details
	tunnel_entry->remote_ip		=	req->drb_dl_ip_addr;
	tunnel_entry->remote_te_id	=	req->drb_dl_teid;
	tunnel_entry->tunnel_type	=	TUNNEL_TYPE_DRB;
	tunnel_entry->pdus_info.drb_id  = 	req->drb_id;
	// TD many more field to fill

	drb_succ_rsp_create(tunnel_entry,rsp);
	ue_ctx->drb_tunnel_list[ue_ctx->drb_count] = tunnel_entry;
	ue_ctx->drb_count++;
	return PFM_SUCCESS;
}


static void 
drb_succ_rsp_create(tunnel_t* tunnel_entry,drb_setup_succ_rsp_info_t* rsp)
{
	rsp->drb_id		= tunnel_entry->drb_id;
	rsp->drb_ul_ip_addr	= tunnel_entry->drb_info.ip_addr;
	rsp->drb_ul_teid	= tunnel_entry->drb_info.te_id;
	return;
}

void 
drb_fail_rsp_create(drb_setup_req_info_t *req,drb_setup_fail_rsp_info_t *rsp)
{
	rsp->drb_id = req->drb_id;
	// TD find way to assign cause 
	rsp->cause  = 1;
}
