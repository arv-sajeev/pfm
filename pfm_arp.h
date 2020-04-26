#ifndef __PFM_ROUTE_H__
#define __PFM_ROUTE_H__ 1

#include <rte_jhash.h>
#include <pfm_comm.h>

typedef struct arp_table_entry {
	ipv4_addr_t dst_ip_addr;
	rte_ether_addr mac_addr;
	uint32_t link_id;

} arp_t;

#define MAX_ARP_TABLE_ENTRIES 32
#define HASH_NAME "ARP_TABLE_HASH"
#define HASH_KEY_LEN 32
#define HASH_SEED 26

struct rte_hash_parameters hash_params = 
{
	.name 			= HASH_NAME,
	.entries 		= MAX_ROUTE_TABLE_ENTRIES,
	.reserved 		= 0,
 	.key_len    		= HASH_KEY_LEN,
	.hash_func		= rte_jhash,
	.hash_func_init_val 	= 26,
	.socket_id		= (int)rte_socket_id()
}



