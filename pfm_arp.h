#ifndef __PFM_ROUTE_H__
#define __PFM_ROUTE_H__ 1

#include <rte_jhash.h>
#include "pfm_comm.h"
#include <rte_timer.h>
#include <rte_hash.h>

#define PFM_ARP_TABLE_ENTRIES 32
#define PFM_ARP_HASH_NAME "ARP_TABLE_HASH"
#define PFM_ARP_HASH_KEY_LEN 32
#define HASH_SEED 26

typedef struct arp_table_entry {

	ipv4_addr_t dst_ip_addr;
	struct rte_ether_addr mac_addr;
	uint32_t link_id;
	struct rte_timer* refresh_timer;
	uint8_t try_count;

} arp_entry_t;

typedef struct arp_table {
	struct rte_hash *hash_mapper;
	arp_entry_t entries[PFM_ARP_TABLE_ENTRIES];
} arp_table_t;

typedef struct callback_args	{
	int key;
	uint64_t ticks;	
	struct rte_ring rx_tx_ring;
} cb_args;

int arp_init(void);

static int arp_clear_entry(int key);

static int arp_request_send(int key);

static void refresh_callback(struct rte_timer* timer,void * called_args);

int arp_process_reply(struct rte_mbuf *pkt);


#endif
