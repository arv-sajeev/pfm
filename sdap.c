#include "pfm.h"
#include "tunnel.h"
#include "gtp.h"
#include "pdcp.h"
#include "sdap.h"

void pdcp_data_ind(tunnel_t *drb_tunnel,int flow_id, struct rte_mbuf *mbuf)
{
	// map DRB Tunnel 'drb_tunnel' to PDUS Tunel 'pdus_tunnel' as below
	/* May be 'uint32_t mapped_pdus_idx' need to be change to
		'tunnel_t * mapped_pdus_tunnel'

	  Also 'mapped_flow_idx' to mapped_flow_id'
	*/
	/*
	tunnel_t *pdus_tunnel;
	int flow_id;
	pdus_tunnel = drb_tunnel->drb_info.mapped_pdus_tunnel;
	flow_id = drb_tunnel->drb_info.mapped_flow_id;

	gtp_data_req(pdus_tunnel, flow_id, mbuf);
	*/
}
void gtp_sdap_data_ind(tunnel_t *pdus_tunnel, int flow_id, struct rte_mbuf *mbuf)
{
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
