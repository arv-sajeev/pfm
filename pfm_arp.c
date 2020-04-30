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

#include "pfm_comm.h"
#include "pfm.h"
#include "pfm_log.h"
#include "pfm_arp.h" 


static arp_table_t arp_table;
static int arp_in = 0;

int 
arp_init(void)	{
	if (arp_in !=0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "ARP table already initialised");
	}

	arp_in = 0;
	struct rte_hash_parameters hash_params = {
		.name			=	PFM_ARP_HASH_NAME,
		.entries 		= 	PFM_ARP_TABLE_ENTRIES,
		.reserved		= 	0,
		.key_len		=	PFM_ARP_HASH_KEY_LEN,
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};

	arp_table.hash_mapper	=	rte_hash_create(&hash_params);
	if (arp_table.hash_mapper == NULL)	{
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
	return 0;
}

static int 
arp_clear_entry(int key){
	int ret;
	struct rte_ether_addr empt_mac = {
					.addr_bytes = {0x00,0x00,0x00,0x00,0x00,0x00}
				};
	ipv4_addr_t empt_ip = 		{
					.addr_bytes = {0x00,0x00,0x00,0x00}
				};

	rte_ether_addr_copy(&empt_mac,&(arp_table.entries[key].mac_addr));
	ipv4_addr_copy(&empt_ip,&(arp_table.entries[key].dst_ip_addr));
	arp_table.entries[key].try_count = 0;
	arp_table.entries[key].link_id = 0;
	pfm_trace_msg("Cleared arp entries");
	ret = rte_timer_stop(arp_table.entries[key].refresh_timer);
	if (ret != 0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Unable to clear entry");	
		return -1;
	}
	pfm_trace_msg("Stopped timer");
	return 0;
}

static int 
arp_request_send(int key)	{
	struct rte_mbuf *pkt = rte_mbuf_raw_alloc(sys_info_g.mbuf_pool);
	unsigned char* packet = rte_pktmbuf_mtod(pkt,unsigned char*);
	// Pending 
	packet[0] = arp_table.entries[key].mac_addr.addr_bytes[0];
	packet[1] = arp_table.entries[key].mac_addr.addr_bytes[1];
	packet[2] = arp_table.entries[key].mac_addr.addr_bytes[2];
	packet[3] = arp_table.entries[key].mac_addr.addr_bytes[3];
	packet[4] = arp_table.entries[key].mac_addr.addr_bytes[4];
	packet[5] = arp_table.entries[key].mac_addr.addr_bytes[5];

	int ret_tx = rte_ring_enqueue_burst(sys_info_g.tx_ring_ptr,
                                            (void *)pkt,
                                            1,
                                            NULL);
	pfm_trace_msg("Sent an arp packet");
	return 0;
}

static void 
refresh_callback(__rte_unused struct rte_timer * timer,void * called_args)	{
	cb_args* args = (cb_args*)called_args;
	int key = args->key;
	uint64_t ticks = args->ticks;
	int ret;
	pfm_trace_msg("ARP entry timed out refresh call attempt :: %d",
		      arp_table.entries[args->key].try_count++);
	// If within retry limit retry and send an arp request
	if (arp_table.entries[args->key].try_count < 4)	{
		args->ticks = args->ticks*2;
		// Send an arp request
		// PENDING
		arp_request_send(key);
		rte_timer_reset(arp_table.entries[key].refresh_timer,
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				refresh_callback,
				(void *)args);
	}

	// Else delete the entry and kill the timer
	else {
		ret = arp_clear_entry(key);
		if (ret != 0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Error clearing arp entry");
		}
		pfm_trace_msg("Cleared arp entry");
	}
}

int 
arp_process_reply(struct rte_mbuf *pkt)	{

	ipv4_addr_t src_ip_addr;
	struct rte_ether_addr src_mac_addr;
	int ret,key;
	uint64_t ticks;
	unsigned char* packet = rte_pktmbuf_mtod(pkt,unsigned char*);

	packet += pkt->l2_len;
	
	src_mac_addr.addr_bytes[0] = packet[8];
	src_mac_addr.addr_bytes[1] = packet[9];
	src_mac_addr.addr_bytes[2] = packet[11];
	src_mac_addr.addr_bytes[3] = packet[12];
	src_mac_addr.addr_bytes[4] = packet[13];
	src_mac_addr.addr_bytes[5] = packet[14];

	src_ip_addr.addr_bytes[0]  = packet[15];
	src_ip_addr.addr_bytes[1]  = packet[16];
	src_ip_addr.addr_bytes[2]  = packet[17];
	src_ip_addr.addr_bytes[3]  = packet[18];


	// 1.	Check whether a hash key of the same value exist
	uint32_t ip_lp = (uint32_t)src_ip_addr.addr_bytes;
	key = rte_hash_lookup_data(arp_table.hash_mapper,
				   (void *) &ip_lp,
				   NULL);
	// 	1.a. 	If already exists check if same value is in the table
	if (key >= 0)	{
	// 	1.b	If same value for src_ip just update the entry with new values
		if (ipv4_addr_equal(&src_ip_addr,&(arp_table.entries[key].dst_ip_addr)))	{
			rte_ether_addr_copy(&src_mac_addr,&(arp_table.entries[key].mac_addr));
			//ACHA
			arp_table.entries[key].link_id = 0;
			arp_table.entries[key].try_count = 0;
			// Reset the timer
			// CHECK just reset will  work??
			rte_timer_reset(arp_table.entries[key].refresh_timer,
					0,
					PERIODICAL,
					rte_lcore_id(),
					NULL,
					NULL);
			pfm_trace_msg("Updated ARP ENTRY :: %d.%d.%d%d	-"
				      "-  %d:%d:%d:%d:%d:%d  |",
				      src_ip_addr.addr_bytes[0],
				      src_ip_addr.addr_bytes[1],
				      src_ip_addr.addr_bytes[2],
				      src_ip_addr.addr_bytes[3],
				      src_mac_addr.addr_bytes[0],
				      src_mac_addr.addr_bytes[1],
				      src_mac_addr.addr_bytes[2],
				      src_mac_addr.addr_bytes[3],
				      src_mac_addr.addr_bytes[4],
				      src_mac_addr.addr_bytes[5]);

			return 0;
		}
	}
	// 	1.c 	If not same value it is a collision
	// 2.	If it doesnt exist check if full
	ret = rte_hash_count(arp_table.hash_mapper);
	if (ret == PFM_ARP_TABLE_ENTRIES)	{
	//	2.a 	If full log 
	//	2.b 	Find a way to make an ARP request
		pfm_log_msg(PFM_LOG_WARNING,
			    "ARP table full");
		return -1;

	}
	// 3	If not full
	// 	3.a	Add computer hash to act as index to the table
	key = rte_hash_add_key(arp_table.hash_mapper,
			       (void *) &ip_lp);
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
	rte_ether_addr_copy(&src_mac_addr,&(arp_table.entries[key].mac_addr));
	ipv4_addr_copy(&src_ip_addr,&(arp_table.entries[key].dst_ip_addr));
	//Acha
	arp_table.entries[key].link_id = 0;
	arp_table.entries[key].try_count = 0;
	
	pfm_trace_msg("Added ARP ENTRY :: %d.%d.%d%d	-"
		      "-  %d:%d:%d:%d:%d:%d  |",
		      src_ip_addr.addr_bytes[0],
		      src_ip_addr.addr_bytes[1],
		      src_ip_addr.addr_bytes[2],
		      src_ip_addr.addr_bytes[3],
		      src_mac_addr.addr_bytes[0],
		      src_mac_addr.addr_bytes[1],
		      src_mac_addr.addr_bytes[2],
		      src_mac_addr.addr_bytes[3],
		      src_mac_addr.addr_bytes[4],
		      src_mac_addr.addr_bytes[5]);
	//Refresh timeout for 60 seconds
	
	ticks = 60*rte_get_timer_hz();

	cb_args refresh_args = {
					.key = key,
					.ticks = ticks
				};

	rte_timer_init(arp_table.entries[key].refresh_timer);
	rte_timer_reset(arp_table.entries[key].refresh_timer,
			ticks,
			PERIODICAL,
			rte_lcore_id(),
			refresh_callback,
			(void *)&refresh_args);
	pfm_trace_msg("Processed ARP reply");
	return 0;
}
