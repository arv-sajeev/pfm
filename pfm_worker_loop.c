#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_distributor.h>


#include "pfm.h"
#include "pfm_log.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "pfm_kni.h"
#include "pfm_arp.h"
#include "pfm_route.h"
#include "pfm_link.h"
#include "pfm_worker_loop.h"

#define TX_QUEUE_SIZE	TX_BURST_SIZE
#define MAX_PKT_PER_TX	8

typedef struct 
{
	struct rte_mbuf *tx_queue[TX_QUEUE_SIZE];
	int tx_queue_head;
	int tx_queue_tail;
	int packet_id;
} worker_info_t;

static worker_info_t worker_info_g[MAX_WORKERS];

static int get_pkt_to_tx(worker_info_t *w,
		struct rte_mbuf *tx_pkts[], int max_pkt)
{
	int tx_sz = 0;
	while(w->tx_queue_head != w->tx_queue_tail)
	{
		if (tx_sz >= max_pkt)
			break;
		tx_pkts[tx_sz] = w->tx_queue[w->tx_queue_tail];
		tx_sz++;

		w->tx_queue_tail++;
		if (TX_QUEUE_SIZE == w->tx_queue_tail)
			w->tx_queue_tail = 0;
	}

	return tx_sz;
}
static void queue_pkt_to_tx(worker_info_t *w, struct rte_mbuf *tx_pkts)
{
	int last_tx_queue_head = w->tx_queue_head;
	w->tx_queue[w->tx_queue_head] = tx_pkts;
	w->tx_queue_head++;
	if (TX_QUEUE_SIZE == w->tx_queue_head)
		w->tx_queue_head = 0;
	if(w->tx_queue_head == w->tx_queue_tail)
	{
		pfm_log_msg(RTE_LOG_ERR,
		    "TX Queue full at lcore %d. Max queue size=%d",
		    rte_lcore_id(),TX_QUEUE_SIZE);

		w->tx_queue_head = last_tx_queue_head;
		return;
	}

	return ;
}

int worker_loop( __attribute__((unused)) void *args)
{
	struct rte_mbuf *rx_pkts[TX_BURST_SIZE];
	struct rte_mbuf *tx_pkts[TX_BURST_SIZE];
	struct rte_ipv4_hdr     *ip_hdr_ptr;
	struct rte_udp_hdr     *udp_hdr_ptr;
	pfm_ip_addr_t	src_addr;
	pfm_ip_addr_t	dst_addr;
	pfm_protocol_t protocol;
	int src_port_no;
	int dst_port_no;
	int pkt_hdr_len;
	int worker_id;
	int lcore_id;
	worker_info_t *wkr;
	unsigned char *pkt;
	uint16_t rx_sz = 0;
	uint16_t tx_sz = 0;
	int ret;

	lcore_id = rte_lcore_id();
	worker_id = (lcore_id - LCORE_WORKER);
	if (MAX_WORKERS <= worker_id)
	{
		pfm_log_rte_err(RTE_LOG_ERR,
		   "Attempt to create worker with Id=%d failed."
			"Only %d worker threads are allowed",
			worker_id,MAX_WORKERS);
		return 1;
	}

	wkr = &worker_info_g[worker_id];
	wkr->tx_queue_head=0;
	wkr->tx_queue_tail=0;
	wkr->packet_id=0;

	pfm_trace_msg("Started worker %d on lcore [%d]. WorkerPtr=%p",
		worker_id,lcore_id,wkr);

	while (force_quit_g != PFM_TRUE)	{

		tx_sz = get_pkt_to_tx(wkr,tx_pkts,MAX_PKT_PER_TX);

		/* Receive packets to be processed and send back 
		   packets that have completed processing  */
		rx_sz = rte_distributor_get_pkt(
					sys_info_g.dist_ptr,
					worker_id,  
					rx_pkts,
					tx_pkts,
					tx_sz);
		if (tx_sz > 0 )	{

			pfm_trace_msg("Sent %d packets back to "
					"distributor from worker core [%d]",
					tx_sz, lcore_id);
			printf("Sent %d packets back to "
					"distributor from worker core [%d]",
					tx_sz, lcore_id);
		}


		if (rx_sz > 0 )	{

			pfm_trace_msg("Received %d packets from distributor"
					"on worker core [%d]",
					rx_sz,
					lcore_id);
			printf("Received %d packets from distributor"
					"on worker core [%d]",
					rx_sz,
					lcore_id);
		}

		/* Clear any accumulated packets */
		while(1)
		{
			tx_sz = get_pkt_to_tx(wkr,tx_pkts,MAX_PKT_PER_TX);
			if (0 == tx_sz) break;
			ret = rte_distributor_return_pkt(
				sys_info_g.dist_ptr,
				worker_id,
				tx_pkts,
				tx_sz);
			if (0 != ret)
			{
				pfm_log_rte_err(RTE_LOG_ERR,
				   "rte_distributor_return_pkt() failed");
			}
		};


		for (uint16_t i = 0;i < rx_sz;i++)
		{
			pkt = rte_pktmbuf_mtod(rx_pkts[i],unsigned char *);
			pkt_hdr_len = sizeof(struct rte_ether_hdr);

			ip_hdr_ptr =
				(struct rte_ipv4_hdr *) (pkt + pkt_hdr_len);
			pkt_hdr_len += sizeof(struct rte_ipv4_hdr);

			dst_addr = ntohl(ip_hdr_ptr->dst_addr);
			src_addr = ntohl(ip_hdr_ptr->src_addr);
			protocol = ip_hdr_ptr->next_proto_id;
			switch(protocol)
			{
				case PROTOCOL_UDP:
					udp_hdr_ptr=(struct rte_udp_hdr *)
					    ( pkt + pkt_hdr_len);
					pkt_hdr_len +=
					    sizeof(struct rte_udp_hdr);
					src_port_no=ntohs(udp_hdr_ptr->src_port);
					dst_port_no=ntohs(udp_hdr_ptr->dst_port);
					break;
				default:
				    pfm_log_msg(RTE_LOG_ERR,
					"Rcvd pkt of Protocol=%d "
					"which is not implemented."
					"Pkt dropped",protocol);
					rte_pktmbuf_free(rx_pkts[i]);
					continue;
			}

			pkt = (unsigned char *)
				rte_pktmbuf_adj(rx_pkts[i],pkt_hdr_len);
			if (NULL == pkt)
			{
				pfm_log_msg(RTE_LOG_ERR,
					"Packet too small pkrLen=%d."
					" Discarded",rx_pkts[i]->pkt_len);
				rte_pktmbuf_free(rx_pkts[i]);
				continue;
			}
			
			pfm_data_ind(src_addr, dst_addr,
				protocol,
				src_port_no, dst_port_no,
				rx_pkts[i]);
		}
	}
	pfm_log_msg(RTE_LOG_CRIT,
		    "Exiting worker thread on lcore [%d]", lcore_id);
	return 1;
}


static const unsigned char *src_mac_addr_get(pfm_ip_addr_t local_ip_addr)
{

	return kni_ipv4_mac_addr_get(local_ip_addr);
}

static const unsigned char *dst_mac_addr_get(pfm_ip_addr_t remote_ip_addr,
			int *link_id)
{

	pfm_arp_entry_t *arp_ptr;
	route_t	*route_ptr;
	// Check if entry available in arp table as next hop address
	arp_ptr = pfm_arp_query(remote_ip_addr);
	if (NULL != arp_ptr)
	{	
		
		*link_id = arp_ptr->link_id;
		return arp_ptr->mac_addr.addr_bytes;
	}

	// Else check for default for the ip address in the lpm table
	route_ptr = pfm_route_query(remote_ip_addr);
	if (NULL != route_ptr)
	{
		arp_ptr =
		   pfm_arp_query(route_ptr->gateway_addr);
		if (NULL != arp_ptr)
		{
			*link_id = arp_ptr->link_id;
			return arp_ptr->mac_addr.addr_bytes;
		}
	}

	return NULL;
}

/**************************
 *
 * 	check_sum
 *
 * 	Used to calculate the IP checksum for bytes specified.
 * 	1's complement addition of 16 bit words
 *
 * 	Arguments	:	pkt	-	pointer to packet buffer
 * 				pkt_len	-	packet length
 *
 * 	Return		:	16 bit check sum value
 *
 ***************************/

static short int check_sum(void *pkt,const int pkt_len)	{
	unsigned char *addr = (unsigned char *)pkt;
	unsigned int csum = 0;
	unsigned short ret;
	int i = 0;

	// Add each word in the header (16 bit addition
	for (i=0;i <= pkt_len;i += 2)	{
		// Making it 16 byte addition 
		csum += (addr[i] << 8) + addr[i+1]; 
	}
	//In the case that there are odd number of packets pad with zero byte
	if (pkt_len % 2 == 0)	{	
		csum += (addr[i]<<8) + 0x00;
	}
	// get rid of carry by folding 
	
	while (csum >> 16)	{
		csum = (csum & 0xFFFF) + (csum>>16);
	}

	ret = ~csum;
	return ret;
}


void pfm_data_req(      const pfm_ip_addr_t local_ip_addr,
			const pfm_ip_addr_t remote_ip_addr,
			pfm_protocol_t protocol,
			const int local_port_no,
			const int remote_port_no,
			struct rte_mbuf *mbuf)
{
	unsigned char remote_mac_addr[MAC_ADDR_SIZE] =
			{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	char ip_str[STR_IP_ADDR_SIZE+1];
	struct rte_ether_hdr	*ether_hdr_ptr;
	struct rte_ipv4_hdr	*ipv4_hdr_ptr;
	struct rte_udp_hdr	*udp_hdr_ptr;
	unsigned char *mac_addr;
	unsigned char *pkt;
	int pkt_hdr_len;
	int pkt_len;
	int lcore_id;
	int worker_id;
	int link_id;
	char *p;

	worker_info_t *wkr;
	lcore_id = rte_lcore_id();
	worker_id = (lcore_id - LCORE_WORKER);
	wkr = &worker_info_g[worker_id];
	
	pkt_hdr_len =	sizeof(struct rte_ether_hdr) +
			sizeof(struct rte_ipv4_hdr) +
			sizeof(struct rte_udp_hdr);
	pkt_len = mbuf->pkt_len;

	p = rte_pktmbuf_prepend(mbuf,pkt_hdr_len);
	if (NULL == p)
	{
		pfm_log_msg(RTE_LOG_ERR,
			"No space in headroom to add "
			"Eth/IP/UDP header. Tx pkt Discarded");
		rte_pktmbuf_free(mbuf);
		/* TODO need to add code to allocate
		   new buffer and attached it to this buffer
		   so that there is spece for header */
		return;
	}

	pkt = rte_pktmbuf_mtod(mbuf,unsigned char *);
	ether_hdr_ptr = (struct rte_ether_hdr *)pkt;
	ipv4_hdr_ptr = (struct rte_ipv4_hdr *)
				(pkt + sizeof(struct rte_ether_hdr));
	udp_hdr_ptr = (struct rte_udp_hdr *)
			( pkt +
			  sizeof(struct rte_ether_hdr) +
			  sizeof(struct rte_ipv4_hdr));

	/*Populate Etherner header */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	mac_addr = src_mac_addr_get(local_ip_addr);
#pragma GCC diagnostic pop
	if (NULL == mac_addr)
	{
		pfm_log_msg(RTE_LOG_ERR,
			"Not able to get SrcMACAddr for IP %s."
			"Dropping Packet",
			pfm_ip2str(local_ip_addr,ip_str));
		rte_pktmbuf_free(mbuf);
		return;
	}
	memcpy(&ether_hdr_ptr->s_addr,
		mac_addr,sizeof(ether_hdr_ptr->s_addr));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	mac_addr = dst_mac_addr_get(remote_ip_addr,&link_id);
#pragma GCC diagnostic pop
	if (NULL == mac_addr)
	{
		pfm_log_msg(RTE_LOG_ERR,
			"Not able to get SrcMACAddr for IP %s."
			"Dropping Packet",
			pfm_ip2str(remote_ip_addr,ip_str));
		rte_pktmbuf_free(mbuf);

		pfm_kni_broadcast_arp(remote_ip_addr, remote_mac_addr);
		return;
	}
	memcpy(&ether_hdr_ptr->d_addr,
		mac_addr,sizeof(ether_hdr_ptr->d_addr));

	ether_hdr_ptr->ether_type = htons(RTE_ETHER_TYPE_IPV4); 

	/*Populate IP header */
	ipv4_hdr_ptr->version_ihl	= (0x40|0x05);
	ipv4_hdr_ptr->type_of_service	= 0;
	ipv4_hdr_ptr->total_length =
		htons(pkt_len + sizeof(struct rte_ipv4_hdr) +
			sizeof(struct rte_udp_hdr));
	ipv4_hdr_ptr->packet_id = htons(((lcore_id << 12) | wkr->packet_id ));
	ipv4_hdr_ptr->fragment_offset = 0;
	ipv4_hdr_ptr->time_to_live = 0;
	ipv4_hdr_ptr->next_proto_id = protocol;
	ipv4_hdr_ptr->hdr_checksum = 0;

	ipv4_hdr_ptr->src_addr = htonl(local_ip_addr);
	ipv4_hdr_ptr->dst_addr = htonl(remote_ip_addr);

	ipv4_hdr_ptr->hdr_checksum = check_sum(ipv4_hdr_ptr,sizeof(struct rte_ipv4_hdr));

	/*Populate UDP header */
	udp_hdr_ptr->src_port = htons(local_port_no);
	udp_hdr_ptr->dst_port = htons(remote_port_no);
	udp_hdr_ptr->dgram_len = htons(pkt_len + sizeof(struct rte_udp_hdr));
	udp_hdr_ptr->dgram_cksum = 0;

	/* Set port_no with LinkId */
	mbuf->port = link_id;
	mbuf->ol_flags |= (PKT_TX_IPV4 | PKT_TX_IP_CKSUM ); // not supported on VM

	queue_pkt_to_tx(wkr,mbuf);
} 
