#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include "gtp.h"
#include "pdcp.h"

void pdcp_data_ind(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{
	// find tunnel_Info of mapped NGu tunnel into
	gtp_data_req(ngut, mbuf);
}
void gtp_sdapdata_ind(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{
	// find tunnel_Info of mapped F1u tunnel into
	pdcp_data_req(f1ut, mbuf);
}
#endif
