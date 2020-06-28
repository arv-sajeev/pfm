#ifndef __SDAP_H__ 1
#define __SDAP_H__

#include "pfm.h"
#include "gtp.h"
#include "pdcp.h"

void pdcp_data_ind(tunnel_t *t, int flow_id, struct rte_mbuf *mbuf);
void gtp_sdap_data_ind(tunnel_t *t, int flow_id, struct rte_mbuf *mbuf);

#endif

