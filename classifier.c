#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_ring.h"
#include "kni.h"
#include "classifier.h"

typedef struct
{
	unsigned char localIpAddr[IP_ADDR_SIZE];
	struct rte_kni *kni_ptr;
	int	gtpPortNo;
} KniMapping_t;

typedef struct
{
	int kni_count;
	KniMapping_t kniMap[MAX_KNI_PORTS];
} IngressClassifier_t;


IngressClassifier_t IngressClassifier[LAST_LINK_ID+1];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void printClassifier(void)
{
	int linkId;
	int kniIdx;
	IngressClassifier_t *p;

	printf("CLASSIFIER:\n");
	for(linkId=0; linkId < LAST_LINK_ID; linkId++)
	{
		p = &IngressClassifier[linkId];
		for (kniIdx=0; kniIdx < p->kni_count;kniIdx++)
		{
			printf("\tLinkId=%d,kni_ptr=%p,gtpPort=%d,"
					"IpAddr=%d.%d.%d.%d\n",
				linkId,	
				p->kniMap[kniIdx].kni_ptr,
				p->kniMap[kniIdx].gtpPortNo,
				p->kniMap[kniIdx].localIpAddr[0],
				p->kniMap[kniIdx].localIpAddr[1],
				p->kniMap[kniIdx].localIpAddr[2],
				p->kniMap[kniIdx].localIpAddr[3]);
		}
	}
}

#pragma GCC diagnostic pop

pfm_retval_t pfm_ingress_classifier_add(const int linkId,
				const char *kniName,
				const unsigned char *localIpAddr,
				const int subnetMaskLen,
				const int gtpPortNo)
{
	static pfm_bool_t firstCall = PFM_TRUE;
	struct rte_kni *kni_ptr;
	int idx;
	IngressClassifier_t *icPtr = &IngressClassifier[linkId];

	if (PFM_TRUE == firstCall)
	{
		/* calling the function 1st time. Init the array */
		pfm_trace_msg("Invoking pfm_ingress_classifier_add() "
				"first time. "
				"Init IngressClassifier[] arrary");
		for (idx=0; idx <= LAST_LINK_ID; idx++)
		{
			IngressClassifier[idx].kni_count = 0;
		}

		/* set to false so that subsequent calls will not 
		   init IngressClassifier[] again */
		firstCall = PFM_FALSE;
	}
	
	kni_ptr = KniOpen(linkId, kniName, localIpAddr, subnetMaskLen);
	if (NULL == kni_ptr)
	{
		pfm_log_msg(PFM_LOG_WARNING,
                                "Failed to add Ingress Classifier "
				"KNI '%s' creation failed",
                                kniName);
		return PFM_FAILED;

	}

	idx = icPtr->kni_count;
	memcpy(icPtr->kniMap[idx].localIpAddr, localIpAddr, IP_ADDR_SIZE);
	icPtr->kniMap[idx].kni_ptr = kni_ptr;
	icPtr->kniMap[idx].gtpPortNo = gtpPortNo;
	icPtr->kni_count++;

	pfm_trace_msg("Ingress Classifier added for KNI %s "
			"with IP=%d.%d.%d.%d, netMaskLen=%d, kni_count=%d"
			"GTPPortNo=%d, kni_ptr=%p",
			kniName,
			icPtr->kniMap[idx].localIpAddr[0],
			icPtr->kniMap[idx].localIpAddr[1],
			icPtr->kniMap[idx].localIpAddr[2],
			icPtr->kniMap[idx].localIpAddr[3],
			subnetMaskLen,
			icPtr->kni_count,
			icPtr->kniMap[idx].gtpPortNo,
			icPtr->kniMap[idx].kni_ptr);
	return PFM_SUCCESS;
}


/**********************
 * packetClassify()
 *
 * This function will classify each receved packet as BROADCAST,
 * GTP or OTHERS. Before the classificaion, the destination IP Addr
 * of the packt is checked to make sure the packet is indeed
 * intended for this device. If the dest IP Addr of the packet
 * does not match with any of the local address, such packets are
 * classified as UNKNOWN.
 *
 * Args:
 *    - icPt	- input. pointer to classifier table
 *    - mbuf	- input. point to packet to be classifed
 *    - pktType	- output. what type of packet is mbug
 *
 * Return - Valid only if pktType is OTHERS. If pkType is OTHERS,
 *  return value provide the pointer to KNIT to which the packet
 *  to be forwared. NULL indicate, failure to execute.
 */
 
#define PKT_CLASS_GTP		1
#define PKT_CLASS_OTHERS	2
#define PKT_CLASS_BROADCAST	3
#define PKT_CLASS_UNKNOWN	4

static struct rte_kni *packetClassify(IngressClassifier_t *icPtr,
				struct rte_mbuf *mbuf,
				int *pktType)
{
	int idx;
	unsigned char *pkt, *tmp;
	struct rte_ipv4_hdr	*ipHdrPtr;
	struct rte_udp_hdr	*udpHdrPtr;
	struct rte_ether_hdr	*ethHdrPtr;
	unsigned char bcastIpAddr[IP_ADDR_SIZE] = {255,255,255,255};
	struct rte_kni *kni_ptr;


	pkt = (unsigned char *)rte_pktmbuf_mtod(mbuf,char *);
	ethHdrPtr = (struct rte_ether_hdr *) pkt;
	ipHdrPtr = (struct rte_ipv4_hdr *)
				(pkt + sizeof(struct rte_ether_hdr));
	udpHdrPtr = (struct rte_udp_hdr *)
				(pkt + sizeof(struct rte_ether_hdr) +
					sizeof(struct rte_ipv4_hdr));

	tmp = (unsigned char *) &ethHdrPtr->ether_type;

	if ((0x08 == tmp[0]) && (0x06 == tmp[1]))
	{
		/* Ether Type 0x0806 is ARP.
		   Any ARP packets are considers as broadcast 
		   classify them as BROADCAST so it can be broadcasted
		   to all KNIs */
		*pktType = PKT_CLASS_BROADCAST;
		return NULL;
	}

	if (!((0x08 == tmp[0]) && (0x00 == tmp[1])))
	{
		/* Ether Type 0x0800 is IPv4.
		   Any packet other tht IPv4 is considers as broadcast 
		   classify them as BROADCAST so it can be broadcasted
		   to all KNIs */
	 	*pktType = PKT_CLASS_BROADCAST;
		return NULL;
	}

	if (0 == memcmp(&ipHdrPtr->dst_addr,bcastIpAddr,IP_ADDR_SIZE))
	{
		/* dest IP addr match with broadcast address 
		   classify it as BROADCAST so it can be broadcasted
		   to all KNIs */
		*pktType = PKT_CLASS_BROADCAST;
		return NULL;
	}

	/* check whether dst IP address match with any of the
	   IP provided to KNI interface */
	for(idx=0; idx < icPtr->kni_count; idx++)
	{
#ifdef TRACE
		unsigned char *p = (unsigned char *) &ipHdrPtr->dst_addr;
		pfm_trace_msg("Comparing IP Add. "
				"Pkt=%d.%d.%d.%d. Tbl=%d.%d.%d.%d. Idx=%d",
				p[0], p[1], p[2], p[3],
				icPtr->kniMap[idx].localIpAddr[0],
				icPtr->kniMap[idx].localIpAddr[1],
				icPtr->kniMap[idx].localIpAddr[2],
				icPtr->kniMap[idx].localIpAddr[3],
				idx);
#endif
		if (0 == memcmp(icPtr->kniMap[idx].localIpAddr,
			&ipHdrPtr->dst_addr,IP_ADDR_SIZE))
		{
			pfm_trace_msg("Both IP's are mathcing");
			/* yet it match with one KNI IP addres */
			break;
		}
		pfm_trace_msg("IP's not mathcing");
	}
	if (idx >= icPtr->kni_count)
	{
		/* the dst ip addr does not match with any
		   KNI IP addr. Hence it is not address to
		   any local address. Classify it as UNKNOWN
		   so it can be dropped.  */		
		pfm_trace_msg("Droping pkts with wrong DstAddr="
				"%d.%d.%d.%d",
				*(unsigned char *)&ipHdrPtr->dst_addr,
				*(((unsigned char *)&ipHdrPtr->dst_addr)+1),
				*(((unsigned char *)&ipHdrPtr->dst_addr)+2),
				*(((unsigned char *)&ipHdrPtr->dst_addr)+3));
		*pktType = PKT_CLASS_UNKNOWN;
		return NULL;
	}

	/* Found a KNI which IP addr same as dst IP addr of received pkt 
	   proceed further to check it is a GTP packet*/
	kni_ptr = icPtr->kniMap[idx].kni_ptr;

	/* if GTP, the protocol field of IP header needs to be UDP */
	if (17 != ipHdrPtr->next_proto_id)
	{
		/* protocol type is not UDP. Hence not GTP packet
		   classify ita OTHERS to that it can be
		   forwarded to maching KNI */
		*pktType = PKT_CLASS_OTHERS;
		return kni_ptr;
	}

	/* It is a UDP packet. Check the dst port is GTP port no */
	if (ntohs(udpHdrPtr->dst_port) == icPtr->kniMap[idx].gtpPortNo)
        {
		/* Dest port is GTP port. Hence classify it as GTP
		   so that it can be forwared to Worker thread */
		*pktType = PKT_CLASS_GTP;
		return kni_ptr;
        }
	/* since not GTP, it need to be passed to KNI.
	   classify it as OTHERS */
	*pktType = PKT_CLASS_OTHERS;
	return kni_ptr;
}


/******************
 * IngressClassify()
 *
 * This function will forward recevied packet based on classification
 * done by packetClassify() function. All GTP packets will be forwared 
 * to workerLoop(). All OTHER packets will be forwared to KNI indentified
 * by packetClassify(). All BROADCAST packets will be duplicated and
 * send to each KNI which is associated to the LinkId. All UNKNOWN packets
 * will be dropped.
 *
 * Args:
 *    linkId	- input. link on which the packet is receved.
 *    pkt	- input. packt bust to be classified and forwared.
 *    numPkts	- input. numbe of packts in the bust.
 *****/
void  IngressClassify(  const int linkId,
			struct rte_mbuf *pkts[],
			const int numPkts)
{
	int idx;
	struct rte_mbuf *ringPkts[RX_BURST_SIZE];
        struct rte_mbuf *kniPkts[RX_BURST_SIZE];
        struct rte_mbuf *kniBCastPkts[2];
        int kniPktCount=0, ringPktCount=0;
	struct rte_kni *kni_ptr;
	struct rte_kni *lastKniPtr=0;
	IngressClassifier_t *icPtr;
	int pktType;
	int ii;

	if (linkId > LAST_LINK_ID)
	{
		pfm_log_msg(PFM_LOG_ERR,
                                "Invalid linkId=%d passed to IngressClassify()"
				" Need to be less that or equal to %d.",
				linkId,LAST_LINK_ID);
		return;
	}

	icPtr = &IngressClassifier[linkId];

	for (idx=0; idx < numPkts; idx++)
	{
		kni_ptr = packetClassify(icPtr,pkts[idx],&pktType);
		switch(pktType)
		{
			case PKT_CLASS_GTP:
				/* accumulate all packets to worker thread
				   in ringPkts[] and send it at the end
				   as one bust */
				pfm_trace_msg(
					"GTP Pkt detected on LinkId=%d, pktId=%d. "
					"Storing as %dth Pkt in bust to Distributor",
					linkId,idx,ringPktCount);
				ringPkts[ringPktCount] = pkts[idx];
				ringPktCount++;
				break;
			case PKT_CLASS_OTHERS:
				/* as much as possible, pkts to the same
				   KNI need to be groupd and send as a
				   single bust */
				if ((lastKniPtr != kni_ptr) &&
						(NULL != lastKniPtr))
				{
					pfm_trace_msg("Flushing %d packets to Kni %p",
						kniPktCount,lastKniPtr);
					KniWrite(kni_ptr,kniPkts,kniPktCount);
					kniPktCount=0;
					lastKniPtr = kni_ptr;
				}
				pfm_trace_msg(
					"Other Pkt detcted on LinkId=%d, pktId=%d. "
					"Storing as %dth Pkt in  bust to Kni %p",
					linkId,idx,kniPktCount,kni_ptr);
				kniPkts[kniPktCount] = pkts[idx];
				kniPktCount++;
				break;
			case PKT_CLASS_BROADCAST:
				pfm_trace_msg(
					"Broadcast Pkt detcted on LinkId=%d, pktId=%d",
					linkId,idx);
				/* All braodcast packet need to be
				   closed and send to all KNI associated
				   to the link */
				for (ii=0; ii < (icPtr->kni_count-1); ii++)
				{
					pfm_trace_msg(
						"Cloning and sending to Kni=%p. Idx=%d",
						icPtr->kniMap[ii].kni_ptr,ii);
					kniBCastPkts[0] = 
						rte_pktmbuf_clone(pkts[idx],
						sys_info_g.mbuf_pool);
					KniWrite(icPtr->kniMap[ii].kni_ptr,
							kniBCastPkts,1);
				}
				if (0 < icPtr->kni_count)
				{
					pfm_trace_msg(
						"Cloning and sending to Kni=%p, Idx=%d",
						icPtr->kniMap[ii].kni_ptr,ii);
					kniBCastPkts[0] = pkts[idx];
					KniWrite(icPtr->kniMap[ii].kni_ptr,
							kniBCastPkts,1);
				}
				break;
			case PKT_CLASS_UNKNOWN:
			default:
				/* unknown packet to be dropped */
				rte_pktmbuf_free(pkts[idx]);
				pfm_log_msg(PFM_LOG_WARNING,
					"Packet dropped at Link=%d. "
					"unknow packet type",linkId);
		}
	}
	if ( 0 < kniPktCount)
	{
		pfm_trace_msg("Flushing %d packets to Kni %p",kniPktCount,kni_ptr);
		/* send remmaing OTHER pkts  accumulted to KNI as a bust */
		KniWrite(kni_ptr, kniPkts,kniPktCount);
	}
	if ( 0 < ringPktCount)
	{
		pfm_trace_msg("Flushing %d packets to Distributer",ringPktCount);
		/* send all accumulted GTP packets to Worker thead */
		ring_write(sys_info_g.rx_ring_ptr,ringPkts,ringPktCount);
	}
	return;
}



