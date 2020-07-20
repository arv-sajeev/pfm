#include <rte_mbuf.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "tunnel.h"
#include "gtp.h"
#include "pdcp.h"
#include "sdap.h"

void pdcp_data_ind(tunnel_t *drb_tunnel,struct rte_mbuf *mbuf)
{
	tunnel_key_t pdus_tunnel_key;
	flow_info_t *flow_info;
	uint32_t drb_flow_idx;
	unsigned char *packet = rte_pktmbuf_mtod(mbuf,unsigned char*);
	char ip_str[STR_IP_ADDR_SIZE+1];

	// Get the pdus tunnel key
	pdus_tunnel_key.ip_addr 	= drb_tunnel->key.ip_addr;
	pdus_tunnel_key.te_id		= drb_tunnel->drb_info.mapped_pdus_idx;
	const tunnel_t *pdus_tunnel 	= tunnel_get(&pdus_tunnel_key);

	// If pdus is invalid log error
	if (pdus_tunnel == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid pdus key :%s %d",
					pfm_ip2str(drb_tunnel->key.ip_addr,ip_str),
					drb_tunnel->drb_info.mapped_pdus_idx);
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

	if (flow_info->flow_allocated != PFM_TRUE ||
		flow_info->mapped_drb_idx != drb_tunnel->key.te_id || 
		flow_info->flow_type != FLOW_TYPE_UL || 
		flow_info->flow_type != FLOW_TYPE_UL_DL 
		)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid drb flow mapping :: %s %d  flow :: %d",
					pfm_ip2str(drb_tunnel->key.ip_addr,ip_str),drb_tunnel->key.te_id,drb_flow_idx);
		return;
	}
	


	// Send packet to next layer UL
	gtp_data_req(pdus_tunnel,drb_flow_idx,mbuf);
	return;
}

void gtp_sdap_data_ind(tunnel_t *pdus_tunnel, uint32_t flow_id, struct rte_mbuf *mbuf)
{
	tunnel_key_t drb_tunnel_key;
	flow_info_t *flow_info;
	char *ret;
	unsigned char *packet;
	char ip_str[STR_IP_ADDR_SIZE+1];

	// Get flow info
	flow_info = &(pdus_tunnel->pdus_info.flow_list[flow_id]);

	// Validate flow details 
	if (flow_info->flow_allocated != PFM_TRUE ||
		flow_info->flow_type != FLOW_TYPE_DL || flow_info->flow_type != FLOW_TYPE_UL_DL)
	{
		pfm_log_msg(PFM_LOG_ERR,"Invalid flow mapping :: %s %d  flow :: %d",
					pfm_ip2str(pdus_tunnel->key.ip_addr,ip_str),pdus_tunnel->key.te_id,flow_id);
		return;
	}
	drb_tunnel_key.ip_addr = pdus_tunnel->key.ip_addr;
	drb_tunnel_key.te_id   = flow_info->mapped_drb_idx;
	const tunnel_t *drb_tunnel = tunnel_get(&drb_tunnel_key);

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
	// map PDU Tunnel 'pdus_tunnel' to DRB Tunnel 'drb_tunnel' as below
	/*
	tunnel_t *drb_tunnel;
	for (i=0; i < pdus_tunnel->pdus_info.flow_count; i++)
	{
		if (pdus_tunnel->pdus_info.flow_list[i].flow_id == flow_id)
		{
			// located to mapping
			drb_tunnel =
			 pdus_tunnel->pdus_info.flow_list[i].mapped_drb_tunnel;
			// need to rename mapped_drb_idx to mapped_drb_tunnel

			pdcp_data_req(drb_tunnel, flow_id,mbuf);
			return;
		}
	}
	*/

	/* Write error log as the mapping is not found */

}
