#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include <rte_timer.h>

#define GTP_HDR_SIZE 8
typedef struct
{
	pfm_ip_addr_t 	remote_ip;
	uint16_t	remote_port_no;
	pfm_ip_addr_t	local_ip;
	uint16_t	local_port_no;
	uint16_t	tunnel_count;
	uint16_t	retry_count;
} 
gtp_path_info_t;

typedef struct
{
	gtp_path_info_t gtp_path;
	struct rte_timer echo_timer;
}
gtp_table_entry_t;

pfm_retval_t gtp_echo_response_send(const gtp_path_info_t *gtp_path,struct rte_mbuf *mbuf);
pfm_retval_t gtp_echo_request_send(gtp_path_info_t *gtp_path);
pfm_retval_t gtp_path_add(pfm_ip_addr_t local_ip,int local_port_no,pfm_ip_addr_t remote_ip,int remote_port_no);
const gtp_path_info_t* gtp_path_get(pfm_ip_addr_t remote_ip);
void gtp_data_req(tunnel_t *t,struct rte_mbuf *mbuf);
pfm_retval_t gtp_path_del(pfm_ip_addr_t remote_ip);

#endif
