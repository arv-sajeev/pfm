#ifndef __SDAP_H__ 
#define __SDAP_H__ 1

#include "pfm.h"
#include "gtp.h"
#include "pdcp.h"

#define SDAP_HDR_SIZE 1
void pdcp_data_ind(tunnel_t *t,struct rte_mbuf *mbuf);
void gtp_sdap_data_ind(tunnel_t *t,uint32_t flow_id,struct rte_mbuf *mbuf);

#endif

