#include <rte_mbuf.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "tunnel.h"
#include "gtp.h"
#include "pdcp.h"
#include "sdap.h"

void pdcp_data_ind(const tunnel_t *drb_tunnel,struct rte_mbuf *mbuf)
{
	const flow_info_t *flow_info;
	uint32_t drb_flow_idx;
	unsigned char *packet = rte_pktmbuf_mtod(mbuf,unsigned char*);
	char ip_str[STR_IP_ADDR_SIZE+1];

	// Get pdus tunnel
	const tunnel_t *pdus_tunnel 	= tunnel_get_with_idx(drb_tunnel->drb_info.mapped_pdus_idx);

	// If pdus is invalid log error
	if (pdus_tunnel == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid pdus index : %d",drb_tunnel->drb_info.mapped_pdus_idx);
		return;
	}
	
	// Decode flow id 
	if (drb_tunnel->drb_info.is_ul_sdap_hdr_enabled)	
	{
		// if sdap ul header is enabled extract flow-id from packet and unwrap
		drb_flow_idx = packet[0] & 0x3F;		// QFI position  bits 2-7
		rte_pktmbuf_adj(mbuf,SDAP_HDR_SIZE);		// REMOVE SDAP header
	}

	else	// flow id can be retrieved from the structure
		drb_flow_idx = drb_tunnel->drb_info.mapped_flow_idx;

	// Validate flow info
	flow_info = &(pdus_tunnel->pdus_info.flow_list[drb_flow_idx]);

	if (flow_info->flow_type != FLOW_TYPE_UL && flow_info->flow_type != FLOW_TYPE_UL_DL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid drb flow mapping :: %s %d  flow :: %d",
		pfm_ip2str(drb_tunnel->key.ip_addr,ip_str),drb_tunnel->key.te_id,drb_flow_idx);
		return;
	}
	
	// Send packet to next layer UL
	gtp_data_req(pdus_tunnel,drb_flow_idx,mbuf);
	return;
}

void gtp_sdap_data_ind(const tunnel_t *pdus_tunnel, uint32_t flow_id, struct rte_mbuf *mbuf)
{
	const flow_info_t *flow_info;
	char *ret;
	unsigned char *packet;
	char ip_str[STR_IP_ADDR_SIZE+1];

	// Get flow info
	flow_info = &(pdus_tunnel->pdus_info.flow_list[flow_id]);

	// Validate flow details 
	if (flow_info->flow_type != FLOW_TYPE_DL && flow_info->flow_type != FLOW_TYPE_UL_DL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid flow mapping :: %s %d  flow :: %d",
					pfm_ip2str(pdus_tunnel->key.ip_addr,ip_str),pdus_tunnel->key.te_id,flow_id);
		return;
	}

	// Get DRB tunnel
	const tunnel_t *drb_tunnel = tunnel_get_with_idx(flow_info->mapped_drb_idx);

	if (drb_tunnel == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid flow mapping :: %s %d  flow :: %d",
			pfm_ip2str(pdus_tunnel->key.ip_addr,ip_str),drb_tunnel->key.te_id,flow_id);
		return;
	}

	// If sdap_dl_header is enabled fill up header
	if (drb_tunnel->drb_info.is_dl_sdap_hdr_enabled)
	{
		ret = rte_pktmbuf_prepend(mbuf,SDAP_HDR_SIZE);
		if (ret == NULL)
		{
			pfm_log_rte_err(PFM_LOG_ERR,"Insufficient HEADROOM");
			return;
		}
		// TODO RDI RQI what are they and how to assign
		packet = rte_pktmbuf_mtod(mbuf,unsigned char *);
		packet[0] = flow_id;
		
	}
	
	pdcp_data_req(drb_tunnel,flow_id,mbuf);
	return;
}
