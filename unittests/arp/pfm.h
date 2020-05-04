#ifndef __PFM_H__
#define __PFM_H__ 1

#include <rte_mbuf.h>
#include "pfm_log.h"

typedef enum
{
	PROTOCOL_UDP    = 17
} pfm_protocol_t;

typedef uint32_t pfm_ip_addr_t;

typedef enum 
{
	PFM_SUCCESS,
	PFM_FAILED,
	PFM_NOT_FOUND
} pfm_retval_t;

typedef enum
{
	PFM_FALSE	= 0,
	PFM_TRUE	= 1
} pfm_bool_t;

pfm_retval_t pfm_init(int argc, char *argv[]);

pfm_retval_t pfm_start_pkt_processing(void);

void pfm_data_ind(      const pfm_ip_addr_t remote_ip_addr,
			const pfm_ip_addr_t local_ip_addr,
			pfm_protocol_t protocol,
			const int remote_port_no,
			const int local_port_no,
			struct rte_mbuf *mbuf);

void pfm_data_req(      const pfm_ip_addr_t local_ip_addr,
			const pfm_ip_addr_t remote_ip_addr,
			pfm_protocol_t protocol,
			const int local_port_no,
			const int remote_port_no,
			struct rte_mbuf *mbuf);
void pfm_terminate(void);
int pfm_link_open(const char *link_name);
void pfm_link_close(const char *link_name);
pfm_retval_t pfm_ingress_classifier_add(const int link_id,
			const char *kni_name,
			pfm_ip_addr_t local_ip_addr,
			const int subnet_mask_len,
			const int port_no);
			
#endif
