#ifndef __KNI_H__
#define __KNI_H__ 1

struct rte_kni *KniOpen(const int linId,
			const char *kniName,
			const unsigned char *ipAddr,
			const int subnetMaskLen);
void KniClose(struct rte_kni *kni);
void KniWrite(struct rte_kni *kni,
		struct rte_mbuf *pktBurst[],
		const unsigned int numPkts);

int KniRead(    struct rte_mbuf *pktBurst[],
                uint16_t burstSize,
                int *linkId);

void KniStateChange(const int linkId,const ops_state_t desiredState);

#endif
