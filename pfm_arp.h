#ifndef __PFM_ROUTE_H__
#define __PFM_ROUTE_H__ 1

#include <rte_jhash.h>
#include "pfm_comm.h"
#include <rte_timer.h>
#include <rte_hash.h>


typedef struct arp_table_entry {

	pfm_ip_addr_t dst_ip_addr;
	pfm_ip_addr_t src_ip_addr;
	struct rte_ether_addr mac_addr;
	uint16_t link_id;
	struct rte_timer* refresh_timer;
	uint8_t try_count;

} arp_entry_t;

typedef struct callback_args	{
	int key;
	uint64_t ticks;	
	struct rte_ring rx_tx_ring;
} cb_args;




int pfm_arp_process_reply(struct rte_mbuf *pkt,uint16_t link_id);

arp_entry_t* pfm_arp_query(pfm_ip_addr_t ip_addr);

void pfm_arp_print(FILE *fp); 


#endif
