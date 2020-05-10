#ifndef __PFM_ARP_H__
#define __PFM_ARP_H__ 1

#include <rte_jhash.h>
#include "pfm_comm.h"
#include <rte_timer.h>
#include <rte_hash.h>


typedef struct arp_table_entry {

	pfm_ip_addr_t dst_ip_addr;
	pfm_ip_addr_t src_ip_addr;
	struct rte_ether_addr mac_addr;
	uint16_t link_id;
	struct rte_timer refresh_timer;
	uint8_t try_count;

} pfm_arp_entry_t;




/************************

	pfm_arp_process_reply
	
	process an arp reply packet received
		- add new entry to arp table 
		- update existing entries
		- entries are cleared after timer associated with each expires

	@params
		pkt	-	the rte_mbuf containing the arp reply packet
		link_id - 	the link id from which the packet was received
	@return 
		0 	- 	normal error free operation 
	       -1	-	errors like ENOSPC while making entry 
		  

*************************/


int pfm_arp_process_reply(struct rte_mbuf *pkt,uint16_t link_id);

/*************************
	pfm_arp_query

	checks and returns whether arp entry for specified target address exists

	@params 
		ip_addr	-	target ip address for arp entry query
	@return 
		pfm_arp_entry_t*	- NULL if entry doesn't exist, else  pointer to arp_entry 

**************************/

pfm_arp_entry_t* pfm_arp_query(pfm_ip_addr_t ip_addr);


/*************************

	pfm_arp_print

	print the arp table into the specified file stream

	@params
		fp	-	filestream pointer to the output file 
	@return
		void	
void pfm_arp_print(FILE *fp); 
*************************/

void pfm_arp_print(FILE *fp);

#endif
