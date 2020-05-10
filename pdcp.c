#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include "gtp.h"
#include "pdcp.h"

void pdcp_data_req(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{
	gtp_data_req(t, mbuf);
}

void gtp_pdcpdata_ind(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{
	pdcp_data_ind(t, mbuf);
}
#endif
