#ifndef __PDCP_H__
#define __PDCP_H__ 1

#include "pfm.h"
#include "gtp.h"
#include "pdcp.h"

void pdcp_data_req(tunnel_t *t, int flow_id, struct rte_mbuf *mbuf);

void gtp_pdcp_data_ind(tunnel_t *t, int flow_id, struct rte_mbuf *mbuf);
#endif
