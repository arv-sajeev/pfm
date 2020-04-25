#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__ 1
#include "pfm.h"
#include "pfm_comm.h"

pfm_retval_t ingress_classifier_add(const int link_id,
                                const char *kni_name,
                                const unsigned char *local_ip_addr,
                                const int subnet_mask_len,
				const int gtp_port_no);

void  ingress_classify(	const int link_id,
				struct rte_mbuf *pkts[],
				const int num_pkts);
#endif
