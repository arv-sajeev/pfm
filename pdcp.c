#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include "tunnel.h"
#include "gtp.h"
#include "pdcp.h"

void pdcp_data_req(const tunnel_t *t, uint32_t flow_id, struct rte_mbuf *mbuf)
{
	/*
	gtp_data_req(t, mbuf);
	*/
}

void gtp_pdcp_data_ind(const tunnel_t *t, uint32_t flow_id, struct rte_mbuf *mbuf)
{
	/*
	pdcp_data_ind(t, mbuf);
	*/
}
#endif
