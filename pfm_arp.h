#ifndef __PFM_ARP_H__ 
#define __PFM_ARP_H__ 1

#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_hash.h>
#include <rte_ether.h>
#include "pfm_comm.h"

#define 

struct arp_table_entry	{
	struct rte_ether_addr eth_addr;
	ipv4_addr_t ip_addr[4];
	int timeout;
};

struct arp_table	{
	struct rte_hash* handler;
	struct arp_entry [0];
};

int arptable_init(int size);

int arptable_addentry(const ipv4_addr_t* ip_addr,
		         const struct rte_ether_addr* mac_addr);

pfm_retval arp_get_nexthop_mac_addr(const ipv4_addr_t* ip_addr,
			            struct rte_ether_addr* mac_addr);

void arp_table_print(const int fd);



