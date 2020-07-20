#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include <rte_timer.h>

#define GTP_HDR_SIZE 8

pfm_retval_t gtp_path_add(pfm_ip_addr_t local_ip,int local_port_no,pfm_ip_addr_t remote_ip,int remote_port_no);
void gtp_data_req(const tunnel_t *t,uint32_t flow_id,struct rte_mbuf *mbuf);
pfm_retval_t gtp_path_del(pfm_ip_addr_t remote_ip);

#endif
