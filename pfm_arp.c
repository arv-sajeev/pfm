#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include <rte_memcpy.h>
#include <rte_random.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "pfm_kni.h"
#include "pfm_comm.h"
#include "pfm.h"
#include "pfm_log.h"
#include "pfm_arp.h" 
#include "pfm_utils.h"

#define PFM_ARP_TABLE_ENTRIES 32
#define PFM_ARP_HASH_NAME "ARP_TABLE_HASH"
#define PFM_ARP_HASH_KEY_LEN 32
#define HASH_SEED 26



static arp_entry_t arp_table[PFM_ARP_TABLE_ENTRIES];
static pfm_bool_t arp_up = PFM_TRUE;
static struct rte_hash* hash_mapper;



int 
arp_init(void)	{
	if (arp_up != PFM_FALSE)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "ARP table already initialised");
	}

	struct rte_hash_parameters hash_params = {
		.name			=	PFM_ARP_HASH_NAME,
		.entries 		= 	PFM_ARP_TABLE_ENTRIES,
		.reserved		= 	0,
		.key_len		=	PFM_ARP_HASH_KEY_LEN,
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};

	hash_mapper	=	rte_hash_create(&hash_params);
	if (hash_mapper == NULL)	{
		pfm_log_msg(PFM_LOG_ALERT,
			    "Error during arp init");
		return -1;
	}
	int ret = rte_timer_subsystem_init();
	if (ret != 0)	{
		if (ret == -ENOMEM)	{
			pfm_log_msg(PFM_LOG_ERR,
				    "Error while initialising timer library");
		}
		return -1;
	}
	pfm_trace_msg("timer library intialised");	
	pfm_trace_msg("arp_table initialized");
	arp_up = PFM_TRUE;
	return 0;
}

static int 
arp_clear_entry(int key){
	int ret;
	struct rte_ether_addr empt_mac = {
					.addr_bytes = {0x00,0x00,0x00,0x00,0x00,0x00}
				};

	rte_ether_addr_copy(&empt_mac,&(arp_table[key].mac_addr));
	arp_table[key].dst_ip_addr = 0;
	arp_table[key].src_ip_addr = 0;
	arp_table[key].try_count = 0;
	arp_table[key].link_id = 0;
	pfm_trace_msg("Cleared arp entries");
	ret = rte_timer_stop(arp_table[key].refresh_timer);
	if (ret != 0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Unable to clear entry");	
		return -1;
	}
	pfm_trace_msg("Stopped timer");
	return 0;
}

//PENDING a way to send arp_request


static void 
refresh_callback(__rte_unused struct rte_timer * timer,void * called_args)	{
	cb_args* args = (cb_args*)called_args;
	int key = args->key;
	uint64_t ticks = args->ticks;
	int ret;
	unsigned char broadcast_addr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	pfm_trace_msg("ARP entry timed out refresh call attempt :: %d",
		      arp_table[args->key].try_count++);
	// If within retry limit retry and send an arp request
	if (arp_table[key].try_count < 4)	{
		args->ticks = args->ticks*2;
		pfm_kni_tx_arp(arp_table[key].link_id,
			       arp_table[key].src_ip_addr,
			       arp_table[key].dst_ip_addr,
			       broadcast_addr
			       );

		rte_timer_reset(arp_table[key].refresh_timer,
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				refresh_callback,
				(void *)args);
	}

	// Else delete the entry and kill the timer
	else {
		pfm_kni_broadcast_arp(arp_table[key].dst_ip_addr,broadcast_addr);
		ret = arp_clear_entry(key);
		if (ret != 0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error clearing arp entry");
		}
		pfm_trace_msg("Cleared arp entry");
	}
}

static void update_callback(__rte_unused struct rte_timer* timer,__rte_unused void* args)	{
	pfm_trace_msg("Updates arp entry");
}



int 
pfm_arp_process_reply(struct rte_mbuf *pkt,uint16_t link_id)	{

	pfm_ip_addr_t src_ip_addr,dst_ip_addr;
	struct rte_ether_addr dst_mac_addr;
	int ret,key;
	uint64_t ticks;
	unsigned char* packet = rte_pktmbuf_mtod(pkt,unsigned char*);

	packet += pkt->l2_len;
	
	// Get the MAC address
	dst_mac_addr.addr_bytes[0] = packet[8];
	dst_mac_addr.addr_bytes[1] = packet[9];
	dst_mac_addr.addr_bytes[2] = packet[11];
	dst_mac_addr.addr_bytes[3] = packet[12];
	dst_mac_addr.addr_bytes[4] = packet[13];
	dst_mac_addr.addr_bytes[5] = packet[14];

	// Get the IP address Pending check if right
	dst_ip_addr  = *(uint32_t *)&packet[15];
	src_ip_addr  = *(uint32_t *)&packet[25];


	// 1.	Check whether a hash key of the same value exist
	key = rte_hash_lookup_data(hash_mapper,
				   (void *) &dst_ip_addr,
				   NULL);
	// 	1.a. 	If already exists check if same value is in the table
	if (key >= 0)	{
	// 	1.b	If same value for src_ip just update the entry with new values
		if (dst_ip_addr == arp_table[key].dst_ip_addr)	{
			rte_ether_addr_copy(&dst_mac_addr,&(arp_table[key].mac_addr));
			//ACHA
			arp_table[key].link_id = link_id;
			arp_table[key].try_count = 0;
			arp_table[key].src_ip_addr = src_ip_addr;
			// Reset the timer
			// CHECK just reset will  work??
			rte_timer_reset(arp_table[key].refresh_timer,
					0,
					PERIODICAL,
					rte_lcore_id(),
					update_callback,
					NULL);

			return 0;
		}
	}
	// 	1.c 	If not same value it is a collision
	// 2.	If it doesnt exist check if full
	ret = rte_hash_count(hash_mapper);
	if (ret == PFM_ARP_TABLE_ENTRIES)	{
	//	2.a 	If full log 
	//	2.b 	Find a way to make an ARP request
		pfm_log_msg(PFM_LOG_WARNING,
			    "ARP table full");
		return -1;

	}
	// 3	If not full
	// 	3.a	Add computer hash to act as index to the table
	key = rte_hash_add_key(hash_mapper,
			       (void *) &dst_ip_addr);
	if (key <0)	{
		if (key == -EINVAL)	{
			pfm_trace_msg("Invalid parameters");
		}
		if (key == -ENOSPC)	{
			pfm_log_msg(PFM_LOG_WARNING,
				    "ARP table is full");
		}
		return -1;
	}

	// 	3.b	Add entry to the table at the index computed
	rte_ether_addr_copy(&dst_mac_addr,&(arp_table[key].mac_addr));
	arp_table[key].dst_ip_addr = dst_ip_addr;
	arp_table[key].src_ip_addr = src_ip_addr;
	//Acha
	arp_table[key].link_id = link_id;
	arp_table[key].try_count = 0;
	
	
	//Start timer for 10 seconds and send an arp request packet
	
	ticks = 10*rte_get_timer_hz();

	cb_args refresh_args = {
					.key = key,
					.ticks = ticks
				};

	rte_timer_init(arp_table[key].refresh_timer);
	rte_timer_reset(arp_table[key].refresh_timer,
			ticks,
			PERIODICAL,
			rte_lcore_id(),
			refresh_callback,
			(void *)&refresh_args);
	pfm_trace_msg("Processed ARP reply");
	return 0;
}

arp_entry_t* 
pfm_arp_query(pfm_ip_addr_t ip_addr)	{

	int ret;
	int key = rte_hash_lookup_data(hash_mapper,
				   (void *)&ip_addr,
				   NULL);
	
	if (key < 0)	{
		if (key == -EINVAL)	{
			pfm_log_msg(PFM_LOG_ERR,
				   "Invalid lookup key");
		}

		if (key == -ENOENT)	{
			pfm_log_msg(PFM_LOG_WARNING,
				    "Entry not found");
		}
		return NULL;
	}

	if (arp_table[key].dst_ip_addr != ip_addr)	{
		pfm_log_msg(PFM_LOG_WARNING,
			    "Entry not found");
		return NULL;
	}

	return &(arp_table[key]);
}


void
pfm_arp_print (FILE * fp)	
{
	char dst_ip_str[STR_IP_ADDR_SIZE],src_ip_str[STR_IP_ADDR_SIZE];
	const void *key_ptr;
	void * data_ptr;
	uint32_t ptr;
	int pos;
	arp_entry_t entry;
	fprintf(fp,"| %-20s | %-25s | %-5s | %-10s | %-20s |\n",
		"Dst Address","HW Address","Link ID","Refresh #","Src address");

	while ((pos = rte_hash_iterate(hash_mapper,&key_ptr,&data_ptr,&ptr)) >= 0)	
	{
		entry = arp_table[pos];
		fprintf(fp,"| %-20s | %02X:%02X:%02X:%02X:%02X:%02X | %-5d | %-10d | %-20s |\n",
			pfm_ip2str(entry.dst_ip_addr,dst_ip_str),
			entry.mac_addr.addr_bytes[0],entry.mac_addr.addr_bytes[1],
			entry.mac_addr.addr_bytes[2],entry.mac_addr.addr_bytes[3],
			entry.mac_addr.addr_bytes[4],entry.mac_addr.addr_bytes[5],
			entry.link_id,
			entry.try_count,
			pfm_ip2str(entry.src_ip_addr,src_ip_str));
	}

}
