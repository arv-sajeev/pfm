#ifndef __PFM_H__
#define __PFM_H__ 1

#include <rte_mbuf.h>
#include "pfm_log.h"


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

typedef struct {
        uint32_t remote_ip_addr;
        uint32_t remote_ip_subnet_mask;    // number of bit in SubNet Mask
        uint16_t remote_gtp_port_num;
} end_point_info_t;

pfm_retval_t pfm_init(int argc, char *argv[]);
pfm_retval_t pfm_end_point_add(	const int link_id,
				const char *kpi_name,
				const uint32_t local_ip_addr,
				const uint16_t local_gtp_port_num,
                        	const end_point_info_t ep[],
				const int ep_count);

pfm_retval_t pfm_start_pkt_processing(void);

void pfm_data_ind(	const uint32_t local_ip,
			const uint16_t port_num,
			const uint16_t tunnel_id,
			struct rte_mbuf *mbuf);
void pfm_data_req(	const uint32_t remote_ip,
			const uint16_t port_num,
			const uint16_t tunnel_id,
			struct rte_mbuf *mbuf);
void pfm_terminate(void);
int pfm_link_open(const char *link_name);
void pfm_link_close(const char *link_name);
pfm_retval_t pfm_ingress_classifier_add(const int link_id,
			const char *kni_name,
			const unsigned char *local_ip_addr,
			const int subnet_mask_len,
			const int gtp_port_no);
			
#endif
