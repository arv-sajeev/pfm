#ifndef __KNI_H__
#define __KNI_H__ 1

#include "pfm_comm.h"

struct rte_kni *kni_open(const int link_id,
			const char *kni_name,
			const pfm_ip_addr_t ip_addr,
			const int subnet_mask_len);
void kni_close(struct rte_kni *kni);
void kni_write(struct rte_kni *kni,
		struct rte_mbuf *pkt_burst[],
		const unsigned int num_pkts);

int kni_read(    struct rte_mbuf *pkt_burst[],
                uint16_t burst_size,
                int *link_id);
const unsigned char *kni_ipv4_mac_addr_get(pfm_ip_addr_t ip);

void kni_state_change(const int link_id,const ops_state_t desired_state);
void kni_ipv4_list_print(FILE *fp);
void kni_ipv4_show_print(FILE *fp, char *kni_name);
void pfm_kni_tx_arp(    const int link_id,
			const pfm_ip_addr_t sender_ip_addr,
			const pfm_ip_addr_t target_ip_addr,
			const unsigned char *target_mac_addr);
void pfm_kni_broadcast_arp(     const pfm_ip_addr_t target_ip_addr,
			const unsigned char *target_mac_addr);

#endif
