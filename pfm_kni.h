#ifndef __KNI_H__
#define __KNI_H__ 1

struct rte_kni *kni_open(const int link_id,
			const char *kni_name,
			const unsigned char *ip_addr,
			const int subnet_mask_len);
void kni_close(struct rte_kni *kni);
void kni_write(struct rte_kni *kni,
		struct rte_mbuf *pkt_burst[],
		const unsigned int num_pkts);

int kni_read(    struct rte_mbuf *pkt_burst[],
                uint16_t burst_size,
                int *link_id);

void kni_state_change(const int link_id,const ops_state_t desired_state);
void kni_ipv4_list_print(FILE *fp);
void kni_ipv4_show_print(FILE *fp,char *kni_name);
#endif
