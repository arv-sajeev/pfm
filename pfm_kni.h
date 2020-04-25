#ifndef __KNI_H__
#define __KNI_H__ 1

struct rte_kni *kni_open(const int link_id,
			const char *kni_name,
			const unsigned char *ip_addr,
			const int subnet_mask_len);
void KniClose(struct rte_kni *kni);
void KniWrite(struct rte_kni *kni,
		struct rte_mbuf *pkt_burst[],
		const unsigned int num_pkts);

int KniRead(    struct rte_mbuf *pkt_burst[],
                uint16_t burst_size,
                int *link_id);

void KniStateChange(const int link_id,const ops_state_t desired_state);

#endif
