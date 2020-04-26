#ifndef __PFM_ROUTE_H__
#define __PFM_ROUTE_H__ 1

#include <rte_jhash.h>


typedef struct route_table_entry {
	uint16_t 	link_id;
	ipv4_addr_t 	net_mask;
	uint16_t	net_mask_depth;
	ipv4_addr_t	gateway_addr;
} route_t;

#define MAX_ROUTE_TABLE_ENTRIES 32
#define HASH_NAME "ROUTE_TABLE_HASH"
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





/*****************
 *
 * 	route_add
 *
 * 	search for entry exisiting in ROUTE table, if not add a new entry and update LPM
 *
 *	@params
 *	 link_id 	- 	the linkid that is to be updated	(uint16_t)
 *	 net_mask	-	net_mask that is to be updated		(ipv4_addr_t)
 *	 net_mask_depth - 	depth of the net_mask to be updated	(uint16_t)
 *	 gateway_addr	-	address of gatewway to be taken 	(ipv4_addr_t)
 *
 *	@returns
 *	 void
 *
 *
 *****************/


int route_add(uint16_t link_id,ipv4_addr_t net_mask,uint16_t net_mask_depth,ipv4_addr_t gateway_addr);


/********************
 *
 * 	route_query
 *
 * 	search for entry for given ip address and return route info
 *
 * 	@params
 * 	 ip_addr	-	key to query route table		(ipv4_addr_t)
 * 
 * 	@returns 
 * 	 the route table entry if found, null other wise 		(route_t)
 *
 *********************/

route_t *route_query(ipv4_addr_t ip_addr);

/**********************
 *
 * 	route_print
 *
 * 	list all the current table entrie in route table, and print to the file stram provided
 *
 * 	@params
 *	 fp		- 	File stream to print the output to 	(FILE *)
 *
 **********************/

void route_print(const FILE *fp);
