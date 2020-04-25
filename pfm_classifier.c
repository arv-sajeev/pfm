#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_ring.h"
#include "pfm_kni.h"
#include "pfm_classifier.h"

typedef struct
{
	unsigned char local_ip_addr[IP_ADDR_SIZE];
	struct rte_kni *kni_ptr;
	int	gtp_port_no;
} kni_mapping_t;

typedef struct
{
	int kni_count;
	kni_mapping_t kni_map[MAX_KNI_PORTS];
} ingress_classifier_t;


ingress_classifier_t ingress_classifier[LAST_LINK_ID+1];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void print_classifier(void)
{
	int link_id;
	int kni_idx;
	ingress_classifier_t *p;

	printf("CLASSIFIER:\n");
	for(link_id=0; link_id < LAST_LINK_ID; link_id++)
	{
		p = &ingress_classifier[link_id];
		for (kni_idx=0; kni_idx < p->kni_count;kni_idx++)
		{
			printf("\tLinkId=%d,kni_ptr=%p,gtpPort=%d,"
					"IpAddr=%d.%d.%d.%d\n",
				link_id,	
				p->kni_map[kni_idx].kni_ptr,
				p->kni_map[kni_idx].gtp_port_no,
				p->kni_map[kni_idx].local_ip_addr[0],
				p->kni_map[kni_idx].local_ip_addr[1],
				p->kni_map[kni_idx].local_ip_addr[2],
				p->kni_map[kni_idx].local_ip_addr[3]);
		}
	}
}

#pragma GCC diagnostic pop

pfm_retval_t pfm_ingress_classifier_add(const int link_id,
				const char *kni_name,
				const unsigned char *local_ip_addr,
				const int subnet_mask_len,
				const int gtp_port_no)
{
	static pfm_bool_t first_call = PFM_TRUE;
	struct rte_kni *kni_ptr;
	int idx;
	ingress_classifier_t *ic_ptr = &ingress_classifier[link_id];

	if (PFM_TRUE == first_call)
	{
		/* calling the function 1st time. Init the array */
		pfm_trace_msg("Invoking pfm_ingress_classifier_add() "
				"first time. "
				"Init ingress_classifier[] arrary");
		for (idx=0; idx <= LAST_LINK_ID; idx++)
		{
			ingress_classifier[idx].kni_count = 0;
		}

		/* set to false so that subsequent calls will not 
		   init ingress_classifier[] again */
		first_call = PFM_FALSE;
	}
	
	kni_ptr = kni_open(link_id, kni_name, local_ip_addr, subnet_mask_len);
	if (NULL == kni_ptr)
	{
		pfm_log_msg(PFM_LOG_WARNING,
                                "Failed to add Ingress Classifier "
				"KNI '%s' creation failed",
                                kni_name);
		return PFM_FAILED;

	}

	idx = ic_ptr->kni_count;
	memcpy(ic_ptr->kni_map[idx].local_ip_addr, local_ip_addr, IP_ADDR_SIZE);
	ic_ptr->kni_map[idx].kni_ptr = kni_ptr;
	ic_ptr->kni_map[idx].gtp_port_no = gtp_port_no;
	ic_ptr->kni_count++;

	pfm_trace_msg("Ingress Classifier added for KNI %s "
			"with IP=%d.%d.%d.%d, netMaskLen=%d, kni_count=%d"
			"GTPPortNo=%d, kni_ptr=%p",
			kni_name,
			ic_ptr->kni_map[idx].local_ip_addr[0],
			ic_ptr->kni_map[idx].local_ip_addr[1],
			ic_ptr->kni_map[idx].local_ip_addr[2],
			ic_ptr->kni_map[idx].local_ip_addr[3],
			subnet_mask_len,
			ic_ptr->kni_count,
			ic_ptr->kni_map[idx].gtp_port_no,
			ic_ptr->kni_map[idx].kni_ptr);
	return PFM_SUCCESS;
}


/**********************
 * packet_classify()
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
 *    - pkt_type	- output. what type of packet is mbug
 *
 * Return - Valid only if pkt_type is OTHERS. If pkType is OTHERS,
 *  return value provide the pointer to KNIT to which the packet
 *  to be forwared. NULL indicate, failure to execute.
 */
 
#define PKT_CLASS_GTP		1
#define PKT_CLASS_OTHERS	2
#define PKT_CLASS_BROADCAST	3
#define PKT_CLASS_UNKNOWN	4

static struct rte_kni *packet_classify(ingress_classifier_t *ic_ptr,
				struct rte_mbuf *mbuf,
				int *pkt_type)
{
	int idx;
	unsigned char *pkt, *tmp;
	struct rte_ipv4_hdr	*ip_hdr_ptr;
	struct rte_udp_hdr	*udp_hdr_ptr;
	struct rte_ether_hdr	*eth_hdr_ptr;
	unsigned char bcast_ip_addr[IP_ADDR_SIZE] = {255,255,255,255};
	struct rte_kni *kni_ptr;


	pkt = (unsigned char *)rte_pktmbuf_mtod(mbuf,char *);
	eth_hdr_ptr = (struct rte_ether_hdr *) pkt;
	ip_hdr_ptr = (struct rte_ipv4_hdr *)
				(pkt + sizeof(struct rte_ether_hdr));
	udp_hdr_ptr = (struct rte_udp_hdr *)
				(pkt + sizeof(struct rte_ether_hdr) +
					sizeof(struct rte_ipv4_hdr));

	tmp = (unsigned char *) &eth_hdr_ptr->ether_type;

	if ((0x08 == tmp[0]) && (0x06 == tmp[1]))
	{
		/* Ether Type 0x0806 is ARP.
		   Any ARP packets are considers as broadcast 
		   classify them as BROADCAST so it can be broadcasted
		   to all KNIs */
		*pkt_type = PKT_CLASS_BROADCAST;
		return NULL;
	}

	if (!((0x08 == tmp[0]) && (0x00 == tmp[1])))
	{
		/* Ether Type 0x0800 is IPv4.
		   Any packet other tht IPv4 is considers as broadcast 
		   classify them as BROADCAST so it can be broadcasted
		   to all KNIs */
	 	*pkt_type = PKT_CLASS_BROADCAST;
		return NULL;
	}

	if (0 == memcmp(&ip_hdr_ptr->dst_addr,bcast_ip_addr,IP_ADDR_SIZE))
	{
		/* dest IP addr match with broadcast address 
		   classify it as BROADCAST so it can be broadcasted
		   to all KNIs */
		*pkt_type = PKT_CLASS_BROADCAST;
		return NULL;
	}

	/* check whether dst IP address match with any of the
	   IP provided to KNI interface */
	for(idx=0; idx < ic_ptr->kni_count; idx++)
	{
#ifdef TRACE
		unsigned char *p = (unsigned char *) &ip_hdr_ptr->dst_addr;
		pfm_trace_msg("Comparing IP Add. "
				"Pkt=%d.%d.%d.%d. Tbl=%d.%d.%d.%d. Idx=%d",
				p[0], p[1], p[2], p[3],
				ic_ptr->kni_map[idx].local_ip_addr[0],
				ic_ptr->kni_map[idx].local_ip_addr[1],
				ic_ptr->kni_map[idx].local_ip_addr[2],
				ic_ptr->kni_map[idx].local_ip_addr[3],
				idx);
#endif
		if (0 == memcmp(ic_ptr->kni_map[idx].local_ip_addr,
			&ip_hdr_ptr->dst_addr,IP_ADDR_SIZE))
		{
			pfm_trace_msg("Both IP's are mathcing");
			/* yet it match with one KNI IP addres */
			break;
		}
		pfm_trace_msg("IP's not mathcing");
	}
	if (idx >= ic_ptr->kni_count)
	{
		/* the dst ip addr does not match with any
		   KNI IP addr. Hence it is not address to
		   any local address. Classify it as UNKNOWN
		   so it can be dropped.  */		
		pfm_trace_msg("Droping pkts with wrong DstAddr="
				"%d.%d.%d.%d",
				*(unsigned char *)&ip_hdr_ptr->dst_addr,
				*(((unsigned char *)&ip_hdr_ptr->dst_addr)+1),
				*(((unsigned char *)&ip_hdr_ptr->dst_addr)+2),
				*(((unsigned char *)&ip_hdr_ptr->dst_addr)+3));
		*pkt_type = PKT_CLASS_UNKNOWN;
		return NULL;
	}

	/* Found a KNI which IP addr same as dst IP addr of received pkt 
	   proceed further to check it is a GTP packet*/
	kni_ptr = ic_ptr->kni_map[idx].kni_ptr;

	/* if GTP, the protocol field of IP header needs to be UDP */
	if (17 != ip_hdr_ptr->next_proto_id)
	{
		/* protocol type is not UDP. Hence not GTP packet
		   classify ita OTHERS to that it can be
		   forwarded to maching KNI */
		*pkt_type = PKT_CLASS_OTHERS;
		return kni_ptr;
	}

	/* It is a UDP packet. Check the dst port is GTP port no */
	if (ntohs(udp_hdr_ptr->dst_port) == ic_ptr->kni_map[idx].gtp_port_no)
        {
		/* Dest port is GTP port. Hence classify it as GTP
		   so that it can be forwared to Worker thread */
		*pkt_type = PKT_CLASS_GTP;
		return kni_ptr;
        }
	/* since not GTP, it need to be passed to KNI.
	   classify it as OTHERS */
	*pkt_type = PKT_CLASS_OTHERS;
	return kni_ptr;
}


/******************
 * ingress_classify()
 *
 * This function will forward recevied packet based on classification
 * done by packet_classify() function. All GTP packets will be forwared 
 * to workerLoop(). All OTHER packets will be forwared to KNI indentified
 * by packet_classify(). All BROADCAST packets will be duplicated and
 * send to each KNI which is associated to the LinkId. All UNKNOWN packets
 * will be dropped.
 *
 * Args:
 *    link_id	- input. link on which the packet is receved.
 *    pkt	- input. packt bust to be classified and forwared.
 *    rx_sz	- input. numbe of packts in the bust.
 *****/
void  ingress_classify(  const int link_id,
			struct rte_mbuf *pkts[],
			const int rx_sz)
{
	int idx;
	struct rte_mbuf *ring_pkts[RX_BURST_SIZE];
        struct rte_mbuf *kni_pkts[RX_BURST_SIZE];
        struct rte_mbuf *kni_bCast_pkts[2];
        int kni_pkt_count=0, ring_pkt_count=0;
	struct rte_kni *kni_ptr;
	struct rte_kni *last_kni_ptr=0;
	ingress_classifier_t *ic_ptr;
	int pkt_type;
	int i;

	if (link_id > LAST_LINK_ID)
	{
		pfm_log_msg(PFM_LOG_ERR,
                                "Invalid link_id=%d passed to ingress_classify()"
				" Need to be less that or equal to %d.",
				link_id,LAST_LINK_ID);
		return;
	}

	ic_ptr = &ingress_classifier[link_id];

	for (idx=0; idx < rx_sz; idx++)
	{
		kni_ptr = packet_classify(ic_ptr,pkts[idx],&pkt_type);
		switch(pkt_type)
		{
			case PKT_CLASS_GTP:
				/* accumulate all packets to worker thread
				   in ring_pkts[] and send it at the end
				   as one bust */
				pfm_trace_msg(
					"GTP Pkt detected on LinkId=%d, pktId=%d. "
					"Storing as %dth Pkt in bust to Distributor",
					link_id,idx,ring_pkt_count);
				ring_pkts[ring_pkt_count] = pkts[idx];
				ring_pkt_count++;
				break;
			case PKT_CLASS_OTHERS:
				/* as much as possible, pkts to the same
				   KNI need to be groupd and send as a
				   single bust */
				if ((last_kni_ptr != kni_ptr) &&
						(NULL != last_kni_ptr))
				{
					pfm_trace_msg("Flushing %d packets to Kni %p",
						kni_pkt_count,last_kni_ptr);
					kni_write(kni_ptr,kni_pkts,kni_pkt_count);
					kni_pkt_count=0;
					last_kni_ptr = kni_ptr;
				}
				pfm_trace_msg(
					"Other Pkt detcted on LinkId=%d, pktId=%d. "
					"Storing as %dth Pkt in  bust to Kni %p",
					link_id,idx,kni_pkt_count,kni_ptr);
				kni_pkts[kni_pkt_count] = pkts[idx];
				kni_pkt_count++;
				break;
			case PKT_CLASS_BROADCAST:
				pfm_trace_msg(
					"Broadcast Pkt detcted on LinkId=%d, pktId=%d",
					link_id,idx);
				/* All braodcast packet need to be
				   closed and send to all KNI associated
				   to the link */
				for (i=0; i < (ic_ptr->kni_count-1); i++)
				{
					pfm_trace_msg(
						"Cloning and sending to Kni=%p. Idx=%d",
						ic_ptr->kni_map[i].kni_ptr,i);
					kni_bCast_pkts[0] = 
						rte_pktmbuf_clone(pkts[idx],
						sys_info_g.mbuf_pool);
					kni_write(ic_ptr->kni_map[i].kni_ptr,
							kni_bCast_pkts,1);
				}
				if (0 < ic_ptr->kni_count)
				{
					pfm_trace_msg(
						"Cloning and sending to Kni=%p, Idx=%d",
						ic_ptr->kni_map[i].kni_ptr,i);
					kni_bCast_pkts[0] = pkts[idx];
					kni_write(ic_ptr->kni_map[i].kni_ptr,
							kni_bCast_pkts,1);
				}
				break;
			case PKT_CLASS_UNKNOWN:
			default:
				/* unknown packet to be dropped */
				rte_pktmbuf_free(pkts[idx]);
				pfm_log_msg(PFM_LOG_WARNING,
					"Packet dropped at Link=%d. "
					"unknow packet type",link_id);
		}
	}
	if ( 0 < kni_pkt_count)
	{
		pfm_trace_msg("Flushing %d packets to Kni %p",kni_pkt_count,kni_ptr);
		/* send remmaing OTHER pkts  accumulted to KNI as a bust */
		kni_write(kni_ptr, kni_pkts,kni_pkt_count);
	}
	if ( 0 < ring_pkt_count)
	{
		pfm_trace_msg("Flushing %d packets to Distributer",ring_pkt_count);
		/* send all accumulted GTP packets to Worker thead */
		ring_write(sys_info_g.rx_ring_ptr,ring_pkts,ring_pkt_count);
	}
	return;
}



