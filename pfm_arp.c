#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

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
#define PFM_ARP_HASH_KEY_LEN IP_ADDR_SIZE
#define HASH_SEED 26



static arp_entry_t arp_table[PFM_ARP_TABLE_ENTRIES];
static pfm_bool_t arp_up = PFM_FALSE;
static struct rte_hash* hash_mapper;


/////////////////////////////////////////////////////////////////////////////////////////


static int 
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
	for (int i = 0;i < PFM_ARP_TABLE_ENTRIES;i++)	{
		rte_timer_init(&(arp_table[i].refresh_timer));
	}

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

//////////////////////////////////////////////////////////////////////////////////////////

static int 
arp_clear_entry(arp_entry_t* arp_entry){
	int ret;
	char dst_ip_str[STR_IP_ADDR_SIZE];
	struct rte_ether_addr empt_mac = {
					.addr_bytes = {0x00,0x00,0x00,0x00,0x00,0x00}
				};
	printf("Clearing arp entry for ip :: %s\n",
		pfm_ip2str(arp_entry->dst_ip_addr,dst_ip_str));
	rte_hash_del_key(hash_mapper,(void *)&arp_entry->dst_ip_addr);
	rte_ether_addr_copy(&empt_mac,&(arp_entry->mac_addr));
	arp_entry->dst_ip_addr = 0;
	arp_entry->src_ip_addr = 0;
	arp_entry->try_count = 0;
	arp_entry->link_id = 0;
	pfm_trace_msg("Cleared arp entries");
	
	ret = rte_timer_stop(&arp_entry->refresh_timer);
	if (ret != 0)	{
		pfm_log_msg(PFM_LOG_ERR,
			    "Unable to clear entry");	
		return -1;
	}
	printf("Stopped timer with return :: %d\n",ret);
	pfm_trace_msg("Stopped timer");
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void 
refresh_callback(__rte_unused struct rte_timer * timer,void * args)	{
	int ret;
	char dst_ip_str[STR_IP_ADDR_SIZE];
	unsigned char broadcast_addr[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	arp_entry_t* arp_entry = (arp_entry_t*)args;	
	uint64_t ticks = rte_get_timer_hz();
	ticks = 5*(arp_entry->try_count+1)*ticks;

	pfm_trace_msg("ARP entry :: %s, timed out, refresh attempt #:: %d",
			pfm_ip2str(arp_entry->dst_ip_addr,dst_ip_str),
	  		arp_entry->try_count);
	printf("ARP entry :: %s, timed out, refresh attempt #:: %d\n",
			pfm_ip2str(arp_entry->dst_ip_addr,dst_ip_str),
	  		arp_entry->try_count);

	// If within retry limit retry and send an arp request
	if (arp_entry->try_count < 2)	{
		pfm_kni_tx_arp(arp_entry->link_id,
			       arp_entry->src_ip_addr,
			       arp_entry->dst_ip_addr,
			       broadcast_addr
			       );

		rte_timer_reset_sync(&arp_entry->refresh_timer,
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				refresh_callback,
				(void *)arp_entry);
		arp_entry->try_count++;
	}

	// Else delete the entry and kill the timer
	else {
		pfm_kni_broadcast_arp(arp_entry->dst_ip_addr,broadcast_addr);
		ret = arp_clear_entry(arp_entry);
		if (ret != 0)	{
			pfm_log_msg(PFM_LOG_ERR,
			    "Error clearing arp entry");
			printf("Error cleaning arp entry\n");
			     return;
		}
		pfm_trace_msg("Cleared arp entry");
		printf("Cleared arp entry\n");
	}
}


//////////////////////////////////////////////////////////////////////////////////////////

int 
pfm_arp_process_reply(struct rte_mbuf *pkt,uint16_t link_id)	{

	pfm_ip_addr_t src_ip_addr,dst_ip_addr;
	char dst_ip_str[STR_IP_ADDR_SIZE];
	struct rte_ether_addr dst_mac_addr;
	int ret,key;
	uint64_t ticks;
	unsigned char* packet = rte_pktmbuf_mtod(pkt,unsigned char*);
	pfm_trace_msg("ARP packet received at pfm_arp_process_reply from link %d",
		      link_id);	
	if (arp_up == PFM_FALSE)	{
		pfm_trace_msg("ARP not yet initialised");
		ret = arp_init();
		if (ret != 0)	{
			pfm_log_msg(PFM_LOG_ERR,
				    "Error during arp initialisation");
			return -1;
		}
		pfm_trace_msg("ARP table is up");
	}
	// Get the MAC address
	
	memcpy(dst_mac_addr.addr_bytes,&packet[22],6);

	// Get the IP address Pending check if right
	dst_ip_addr  = ntohl(*(uint32_t *)&packet[28]);
	src_ip_addr  = ntohl(*(uint32_t *)&packet[38]);

	key = rte_hash_add_key(hash_mapper,
			       (void *)&dst_ip_addr);
	// Check if arp table is full 
	if ((key) == -ENOSPC)	{
		pfm_trace_msg("ARP table is full\n");
		return -1;
	}

	arp_entry_t* arp_entry = &arp_table[key];
	// Fill in the arp entry 
	arp_entry->dst_ip_addr = dst_ip_addr;
	arp_entry->src_ip_addr = src_ip_addr;
	rte_ether_addr_copy(&dst_mac_addr,&(arp_entry->mac_addr));
	arp_entry->try_count = 0;

	pfm_trace_msg("ARP entry for ::%s created",
			pfm_ip2str(arp_entry->dst_ip_addr,dst_ip_str));

	ticks = rte_get_timer_hz();
	ticks = 5*ticks;
	
	// Reset the timer
	pfm_trace_msg("Resetting timer : %s",
			pfm_ip2str(arp_entry->dst_ip_addr,dst_ip_str));
	rte_timer_reset_sync(&(arp_entry->refresh_timer),
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				refresh_callback,
				(void*)arp_entry);
	pfm_trace_msg("Timer reset successfully\n");
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
arp_entry_t* 
pfm_arp_query(pfm_ip_addr_t ip_addr)	{

	int ret;
	if (arp_up == PFM_FALSE)	{
		pfm_trace_msg("ARP not yet initialised");
		ret = arp_init();
		if (ret != 0)	{
			pfm_log_msg(PFM_LOG_ERR,
				    "Error during arp initialisation");
			return NULL;
		}
	}
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


	return &(arp_table[key]);
}

//////////////////////////////////////////////////////////////////////////////////////////

void
pfm_arp_print (FILE * fp)	
{
	char dst_ip_str[STR_IP_ADDR_SIZE],src_ip_str[STR_IP_ADDR_SIZE];
	const void *key_ptr;
	void * data_ptr;
	uint32_t ptr = 0,hash_count;
	int pos,ret;
	if (arp_up == PFM_FALSE)	{
		pfm_trace_msg("ARP not yet initialised");
		ret = arp_init();
		if (ret != 0)	{
			pfm_log_msg(PFM_LOG_ERR,
				    "Error during arp initialisation");
			return;
		}
		pfm_trace_msg("ARP is up");
	}

	hash_count = rte_hash_count(hash_mapper);
	if (hash_count == 0)	{
		fprintf(fp,"ARP table is empty\n");
		return;
	}

	fprintf(fp,"Number of entries :: %u\n",hash_count);
	arp_entry_t *entry;
	fprintf(fp,"| %-20s | %-17s | %-5s | %-10s | %-20s |\n",
		"Dst Address","HW Address","Link ID","Refresh #","Src address");

	while ((pos = rte_hash_iterate(hash_mapper,&key_ptr,&data_ptr,&ptr)) >= 0)	
	{
		entry = &arp_table[pos];
		fprintf(fp,"| %-20s | %02X:%02X:%02X:%02X:%02X:%02X | %-7d | %-10d | %-20s |\n",
			pfm_ip2str(entry->dst_ip_addr,dst_ip_str),
			entry->mac_addr.addr_bytes[0],entry->mac_addr.addr_bytes[1],
			entry->mac_addr.addr_bytes[2],entry->mac_addr.addr_bytes[3],
			entry->mac_addr.addr_bytes[4],entry->mac_addr.addr_bytes[5],
			entry->link_id,
			entry->try_count,
			pfm_ip2str(entry->src_ip_addr,src_ip_str));
	}

}
