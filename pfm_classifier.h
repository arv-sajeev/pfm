#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__ 1
#include "pfm.h"
#include "pfm_comm.h"

void  ingress_classify(	const int link_id,
				struct rte_mbuf *pkts[],
				const int num_pkts);
#endif
