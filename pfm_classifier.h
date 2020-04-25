#ifndef __CLASSIFIER_H__
#define __CLASSIFIER_H__ 1
#include "pfm.h"
#include "pfm_comm.h"

pfm_retval_t IngressClassifierAdd(const int linkId,
                                const char *kniName,
                                const unsigned char *localIpAddr,
                                const int subnetMaskLen,
				const int gtpPortNo);

void  IngressClassify(	const int linkId,
				struct rte_mbuf *pkts[],
				const int numPkts);
#endif
