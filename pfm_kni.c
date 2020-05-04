#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h> 
#include <errno.h> 
#include <sys/wait.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_kni.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_utils.h"
#include "pfm_link.h"
#include "pfm_kni.h"


typedef struct 
{
	struct		rte_kni *ptr;
	char		name[RTE_KNI_NAMESIZE+1];
	pfm_ip_addr_t	ip_addr;
	unsigned char	mac_addr[MAC_ADDR_SIZE];
	int		subnet_mask_len;
	int		link_id;
	ops_state_t	ops_state;
} kni_info_t;

static int kni_count_g = 0;
static kni_info_t kni_info_list_g[MAX_KNI_PORTS];
static const unsigned char default_mac_addr_g[MAC_ADDR_SIZE] =
	{ 0x06, 0x10, 0x20, 0x30, 0x40, 0x50 };

/* Callback function which is invoked when MTU is changed*/
static int callback_func_kni_mtu_change(uint16_t kni_id,
		unsigned int new_mtu)
{
	pfm_trace_msg("callback_func_kni_mtu_change(kni_id=%d, "
				"new_mtu=%d) invoked. "
				"But MTU change is not yet implemented",
				kni_id,new_mtu);
	
       
	return 0;
}

static pfm_retval_t  ip_addr_config(	const char *if_name,
			ops_state_t kni_ops_state,
			pfm_ip_addr_t ip_addr,
			const int mask_len)
{
	static char str_ip_addr[STR_IP_ADDR_SIZE+1+3];
	static char ip_str[STR_IP_ADDR_SIZE+1];
	char *operation;
	pid_t pid; 
	int status; 

	/* Convert IP address to string */
	sprintf(str_ip_addr,"%s/%d",pfm_ip2str(ip_addr,ip_str),mask_len);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	if (OPSSTATE_ENABLED == kni_ops_state)
	{
		operation = "add";
	}
	else
	{
		operation = "del";
	}
#pragma GCC diagnostic pop

	/* start a child process */
	pid = fork(); 
	if ((-1) == pid )
	{
		/* fork() failed */
		pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"fork() failed",
				str_ip_addr,
				mask_len,
				if_name);
		return PFM_FAILED;
	}
	pfm_trace_msg("Child prcess fork() sucessful"); 
	if ( 0 == pid )
	{
		/* in the context of child process */	
		/* CMD format is "ip addr add 192.168.121.45/24 dev eth0" */

		const char *const arg_list[] = 
		   { "ip", "addr", operation, str_ip_addr, "dev", if_name, NULL };

		pfm_trace_msg("Invoking execvp(%s %s %s %s %s %s)",
			arg_list[0],arg_list[1],arg_list[2],
			arg_list[3],arg_list[4],arg_list[5]); 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
		execvp(arg_list[0], arg_list); 
#pragma GCC diagnostic pop
		pfm_trace_msg("execvp() done"); 
		exit(0); /* terminate the chiled procss when done */
	} 
	/* in the context of parent process */	
	if (waitpid(pid, &status, 0) > 0)
	{ 
		if (WIFEXITED(status))
		{
			if (WEXITSTATUS(status))
			{
				pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"WIFEXITED() returm failure",str_ip_addr,if_name);
				return PFM_FAILED;
			}
			else
			{
				pfm_trace_msg("IP %s set to KNI %s",
						str_ip_addr,if_name);
				return PFM_SUCCESS;
			}
		}	
		else
		{
			pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"Child did not teminate normaly\n",
				str_ip_addr,if_name);
			return PFM_FAILED;
		}
	} 
	else
	{ 
		// waitpid() failed 
		pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"waitpid() failed",
				str_ip_addr,if_name);
		return PFM_FAILED;
	} 
}

/* Callback function which is invoked when configuring each KNI interface */
static int callback_func_kni_if_config(uint16_t kni_id, uint8_t kni_state)
{
	int link_id;
	ops_state_t new_ops_state;
	int idx;

	if (kni_id >= kni_count_g)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid KniId=%d passed to callback_func_kni_if_config()",
			kni_id);
		return 0;
	}

	if ( NULL == kni_info_list_g[kni_id].ptr) 
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"KNI ptr for kni_id=%d is null",kni_id);
		return 0;
	}

	pfm_trace_msg("Kni %s(%d) state changed to %s",
			kni_info_list_g[kni_id].name,
			kni_id,
			((ETH_LINK_UP == kni_state) ? "UP": "DOWN"));

	if (ETH_LINK_UP == kni_state)
	{
		new_ops_state = OPSSTATE_ENABLED;
		kni_info_list_g[kni_id].ops_state = OPSSTATE_ENABLED;
	}
	else
	{
		new_ops_state = OPSSTATE_DISABLED;
		kni_info_list_g[kni_id].ops_state = OPSSTATE_DISABLED;
	}
	link_id = kni_info_list_g[kni_id].link_id;

	if (OPSSTATE_ENABLED == new_ops_state )
	{
		pfm_trace_msg("At least one KNI for LinkId=%d is UP. "
			"Hence, Enable the Link",link_id);
		/* if any of the kni associated with link is enabled
		   then the link needs to be enabled */
		link_state_change(link_id,OPSSTATE_ENABLED);
	}
	else
	{
		/* if all the knis associated with alink is in diabled state
		   the the link can be dissabled */
		for(idx=0; idx < kni_count_g; idx++)
		{
			if ( kni_info_list_g[idx].link_id == link_id) 
			{
				if(kni_info_list_g[idx].ops_state !=
							OPSSTATE_DISABLED)
				{
					break;
				}
			}
		}
		if (idx < kni_count_g)
		{
			/* atlest one KNI which is mapped to the Link is
			   still in enabled state. Hence the link
			   can not be disabled */
			pfm_trace_msg("At least one KNI of LinkId=%d is UP."
				" Hence, can not disable Link",link_id);
		}
		else
		{
			/* all KNIs of linka are in disabe state.
			   hence disable the link */
			pfm_trace_msg("All KNIs for LinkId=%d are DOWN. "
				"Hence, disable Link",link_id);
			link_state_change(link_id,OPSSTATE_DISABLED);
		}
	}
	pfm_trace_msg("KNI '%s' state changed to %s",
			kni_info_list_g[kni_id].name,
			((new_ops_state == OPSSTATE_ENABLED) ?
				"UP":"DOWN"));
	return 0;
}


static void kni_set_state(const kni_info_t *kni_ptr, const ops_state_t desired_state)
{
	int prev_state, new_state;

	if (NULL == kni_ptr->ptr)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"KNI ptr for KNI=%s is null",kni_ptr->name);
		return;
	}
	new_state = ETH_LINK_DOWN;
	if (OPSSTATE_ENABLED == desired_state)
	{
		new_state = ETH_LINK_UP;
	}

	prev_state = rte_kni_update_link(kni_ptr->ptr,new_state);
	if (0 > prev_state)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"Not able to set Kni state. "
			"rte_kni_update_link(kni=%s,new_state=%s) failed"
			" with ret_val=%d",
			kni_ptr->name,
			((new_state == ETH_LINK_UP) ? "UP":"DOWN"),
			prev_state);
		return;
	}

	if (prev_state == new_state)
	{
		pfm_trace_msg("KNI '%s' state is not changed. "
			"It is still %s.",
			kni_ptr->name,
			(new_state == ETH_LINK_UP) ? "UP":"DOWN");
		return;
	}
	pfm_trace_msg("KNI '%s' state is changed to %s.",
			kni_ptr->name,
			(new_state == ETH_LINK_UP) ? "UP":"DOWN");
	return;
}

void kni_state_change(const int link_id,const ops_state_t desired_state)
{
	int idx;
	static pfm_bool_t first_call = PFM_TRUE;
	pfm_retval_t ret_val;

	for(idx=0; idx < kni_count_g; idx++)
	{
		if ( (kni_info_list_g[idx].link_id == link_id) &&
			(kni_info_list_g[idx].ptr != NULL))
		{
			kni_set_state(&kni_info_list_g[idx],desired_state);
		}
	}

	if (PFM_TRUE != first_call)
	{
		return;
	}

	first_call = PFM_FALSE;

	for(idx=0; idx < kni_count_g; idx++)
	{
		ret_val = ip_addr_config(kni_info_list_g[idx].name,
				kni_info_list_g[idx].ops_state,
				kni_info_list_g[idx].ip_addr, 
				kni_info_list_g[idx].subnet_mask_len);
		if (PFM_SUCCESS != ret_val)
		{
			pfm_log_msg(PFM_LOG_ERR,
				"Failed to %s IP Addr to KNI=%s",
				((kni_info_list_g[idx].ops_state == OPSSTATE_ENABLED) ?
					"ADD" : "REMOVE"),
				kni_info_list_g[idx].name);
		}
	}

	return;
}
struct rte_kni *kni_open(const int link_id,
			const char *kni_name,
			pfm_ip_addr_t ip_addr,
			const int subnet_mask_len)
{
	struct rte_kni *kni_ptr;
	struct rte_kni_conf conf;
	struct rte_kni_ops ops;
	struct rte_eth_dev_info dev_info;
	int ret_val;
	int ret;
	int idx;
        
	/* Initialize KNI subsystem if it is the 1st port*/
	if (0 == kni_count_g)
	{
		ret_val = rte_kni_init(MAX_KNI_PORTS);
		if (0 != ret_val)
		{
			pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_kni_init(%d) failed",MAX_KNI_PORTS);
			return NULL;
		}
		pfm_trace_msg("Initialized KNI subsystem");
	}

	for(idx=0; idx < kni_count_g; idx++)
	{
		if (0 == strcmp(kni_info_list_g[idx].name,kni_name))
		{
			pfm_log_msg(PFM_LOG_WARNING,
				"KNI with name %s already exists. "
				"Hence not created again",
				kni_name);
			return NULL;
		}
	}

	if (kni_count_g >= MAX_KNI_PORTS)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Trying to open too many (%d) KNI ports. "
			"Only %d is allowed. Request failed.",
			(kni_count_g+1),MAX_KNI_PORTS);
		return NULL;
	}


	memset(&conf, 0, sizeof(conf));
	strncpy(conf.name,kni_name,RTE_KNI_NAMESIZE);
	conf.core_id = LCORE_TXLOOP;
	conf.group_id = link_id;
	conf.mbuf_size = RTE_MBUF_DEFAULT_DATAROOM;
	// conf.addr; Not filled
	// conf.id; Not filled
	// conf.force_bind; Not filled

	/* get MAC addres of the associated link */
	ret = rte_eth_macaddr_get(link_id,
                        (struct rte_ether_addr *)&conf.mac_addr);
        if (ret != 0)
        {
		memcpy(conf.mac_addr,default_mac_addr_g,MAC_ADDR_SIZE);
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Not able to get MAC address for link %d. "
			"rte_eth_macaddr_get() failed. "
			"Assumed default MACAddr = "
			"%02X:%02X:%02X:%02X:%02X:%02X",
			link_id,
			conf.mac_addr[0],conf.mac_addr[1],
			conf.mac_addr[2],conf.mac_addr[3],
			conf.mac_addr[4],conf.mac_addr[5]);
        }

	/* Get MTU size of the associated link */
        ret = rte_eth_dev_get_mtu(link_id, &conf.mtu);
        if (ret != 0)
        {
        	conf.mtu = DEFAULT_MTU_SIZE;
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Not able to get MTU Size for link %d. "
			"rte_eth_dev_get_mtu() failed. "
			"Assumed default value MTU =  %d",
			link_id, conf.mtu);
        }

	/* Get min and max MTU size supported by associated link */
	ret = rte_eth_dev_info_get(link_id, &dev_info);
	if (ret == 0)
	{
        	conf.min_mtu = dev_info.min_mtu;
        	conf.max_mtu = dev_info.max_mtu;
	}
	else
	{
        	conf.min_mtu = DEFAULT_MIN_MTU_SIZE;
	        conf.max_mtu = DEFAULT_MAX_MTU_SIZE;
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Not able to get Min MTU and Max MTU size "
			"for link %d. rte_eth_dev_info_get() failed. "
			"Assumed default values "
			"minMTU=%d,maxMTU=%d",
			link_id, conf.min_mtu,conf.max_mtu);
	}

	memset(&ops, 0, sizeof(ops));
	ops.port_id = kni_count_g;
	ops.change_mtu = callback_func_kni_mtu_change;
	ops.config_network_if = callback_func_kni_if_config;
	ops.config_mac_address = NULL;
    	ops.config_promiscusity = NULL;
   	ops.config_allmulticast = NULL;
	kni_ptr = rte_kni_alloc(sys_info_g.mbuf_pool, &conf, &ops);
	if (NULL == kni_ptr)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_kni_alloc() failed", ret_val);
		return NULL;
	}
	pfm_trace_msg("Allocated KNI interface '%s', LinkId=%d."
			"with MAC=%02X:%02X:%02X:%02X:%02X:%02X,"
			"MTU=%d,minMTU=%d,maxMTU=%d",
			kni_name,link_id,
			conf.mac_addr[0],conf.mac_addr[1],
			conf.mac_addr[2],conf.mac_addr[3],
			conf.mac_addr[4],conf.mac_addr[5],
			conf.mtu,
			conf.min_mtu,
			conf.max_mtu);


	kni_info_list_g[kni_count_g].ptr = kni_ptr;
	strncpy(kni_info_list_g[kni_count_g].name,kni_name,RTE_KNI_NAMESIZE);
	kni_info_list_g[kni_count_g].ip_addr= ip_addr;
	kni_info_list_g[kni_count_g].subnet_mask_len = subnet_mask_len;
	kni_info_list_g[kni_count_g].link_id = link_id;
	memcpy(kni_info_list_g[kni_count_g].mac_addr,
				conf.mac_addr,MAC_ADDR_SIZE);
	kni_count_g++;
	return kni_ptr;
}

void kni_close(struct rte_kni *kni)
{
	int ret;
	int idx;

	if (NULL == kni)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"NULL KNI pointer passed to kni_close()");
		return;
	}

	for (idx=0; idx < kni_count_g; idx++)
	{
		if (kni_info_list_g[idx].ptr == kni)
		{
			break;
		}
	}
	if (idx >= kni_count_g)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Unknow KNI pointer %p passed to kni_close()",kni);
		return;
	}
	kni_info_list_g[idx].ptr = NULL;
	ret = rte_kni_release(kni);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,"rte_kni_release() failed");
		return;
	}
	pfm_trace_msg("Released KNI interface '%s(%d)'",
			kni_info_list_g[idx].name, idx);
	return;
}


/********
 * Write a burst of packets to KNI interface
 *
 * Args:
 *	kni	-	INPUT		- pointer to KNI where the packet
 *					  to be send.
 *	pkt_burst-	INPUT		- array of packet to be send
 *	num_pkts	-	INPUT		- number of packes in 'pkt_burst'
 *					  
 *
 * Return: Number of packets actuly written
 ******/
void kni_write(	struct rte_kni *kni,
			struct rte_mbuf *pkt_burst[],
			const unsigned int num_pkts)
{
	unsigned int pkts_send;

	if (NULL == kni)
	{
		/* KNI is not yet openned */
		return;
	}

	/* Burst tx to kni */
	pkts_send = rte_kni_tx_burst(kni, pkt_burst, num_pkts);
	if (pkts_send < num_pkts)
	{
		/* some packets are not send.
		   Drop them by releasing the buffers */
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Dropped %d slow path packets rte_kni_tx_burst()",
			(num_pkts-pkts_send));
		for(;pkts_send < num_pkts; pkts_send++)
		{
			rte_pktmbuf_free(pkt_burst[pkts_send]);
		}
	}
	rte_kni_handle_request(kni);
	
	return;
}


/********
 * Read a burst of packets from KNI interface
 *
 * Args:
 *	pkt_burst-	input/output. read packets are stored in this array
 *	kni	-	input- pointer to KNI which need to be read
 *	num_pkts	-	INPUT		- size of 'pkt_burst'. i.e.
 *					  i.e, max packet to read.
 *
 * Return: Number of packets actuly read
 *
 ******/
int kni_read(	struct rte_mbuf *pkt_burst[],
		uint16_t burst_size,
		int *link_id)
{
	static uint16_t next_read= 0;
        int start;
	int rx_sz;

	start = next_read;
	do
	{
		rx_sz = 0;
		if (NULL != kni_info_list_g[next_read].ptr)
		{
			rte_kni_handle_request(
				kni_info_list_g[next_read].ptr);

			/* check only if the kni is in unlocked state*/
	                if (OPSSTATE_ENABLED ==
	                        kni_info_list_g[next_read].ops_state)
	                {
				if (kni_info_list_g[next_read].ptr != NULL)
				{
					rx_sz = rte_kni_rx_burst(
					     kni_info_list_g[next_read].ptr,
						pkt_burst,
						burst_size);

                     		  	/* store the kni name in output
					   argument */
					*link_id =
					   kni_info_list_g[next_read].link_id;
				};
			};
		}

                /* Next iteration shold check next KNI in round robin.*/
                next_read++;
                if (next_read>= kni_count_g)
                {
                        // wrap around
                        next_read= 0;
                }

                /* if a bust is received, retrun the funtion*/
                if (0 < rx_sz)
                {
                        return rx_sz;
                }

	} while (next_read!= start); 

	/* non of the enabled knis have packets. hence return 0 */

	return 0;
}

void kni_ipv4_list_print(FILE *fp)
{
	int idx;
	char ip_str[STR_IP_ADDR_SIZE+1];

	fprintf(fp,"   %-10s %-18s %-18s %s\n",
		"NAME",
		"IP ADDRESS",
		"MAC ADDRESS",
		"LINK");
	for(idx=0; idx < kni_count_g; idx++)
	{
		fprintf(fp,"   %-10s %-15s/%-2d "
			"%02X:%02X:%02X:%02X:%02X:%02X  %d\n",
			kni_info_list_g[idx].name,
			pfm_ip2str(kni_info_list_g[idx].ip_addr,ip_str),
			kni_info_list_g[idx].subnet_mask_len,
			kni_info_list_g[idx].mac_addr[0],
			kni_info_list_g[idx].mac_addr[1],
			kni_info_list_g[idx].mac_addr[2],
			kni_info_list_g[idx].mac_addr[3],
			kni_info_list_g[idx].mac_addr[4],
			kni_info_list_g[idx].mac_addr[5],
			kni_info_list_g[idx].link_id);
	}
}

void kni_ipv4_show_print(FILE *fp, char *kni_name)
{
	char ip_str[STR_IP_ADDR_SIZE+1];
	int idx;
	int len;

	len = strlen(kni_name);

	for(idx=0; idx < kni_count_g; idx++)
	{
		if ( 0 == strncasecmp(kni_name,
				kni_info_list_g[idx].name, len))
		{
			break;
		}
	}

	if (idx >= kni_count_g)
	{
		fprintf(fp,"IfName '%s' does not exists\n",kni_name);
		return;
	}

	fprintf(fp,"   %-10s- %s\n","Name",kni_info_list_g[idx].name);
	fprintf(fp,"   %-10s- %s/%d\n","IP Addr",
			pfm_ip2str(kni_info_list_g[idx].ip_addr,ip_str),
			kni_info_list_g[idx].subnet_mask_len);
	fprintf(fp,"   %-10s- %02X:%02X:%02X:%02X:%02X:%02X\n",
			"MAC Addr",
			kni_info_list_g[idx].mac_addr[0],
			kni_info_list_g[idx].mac_addr[1],
			kni_info_list_g[idx].mac_addr[2],
			kni_info_list_g[idx].mac_addr[3],
			kni_info_list_g[idx].mac_addr[4],
			kni_info_list_g[idx].mac_addr[5]);
	fprintf(fp,"   %-10s- %d\n","Link Id",kni_info_list_g[idx].link_id);
	fprintf(fp,"   %-10s- %s\n","Ops State",
		((kni_info_list_g[idx].ops_state==OPSSTATE_ENABLED) ?
				"ENABLED" : "DISABLED"));
	fprintf(fp,"   %-10s- %p\n","KNI Ptr",kni_info_list_g[idx].ptr);
	return;
}

const unsigned char *kni_ipv4_mac_addr_get(pfm_ip_addr_t ip)
{
	int idx;

	for(idx=0; idx < kni_count_g; idx++)
	{
		if (kni_info_list_g[idx].ip_addr == ip)
		{
			return kni_info_list_g[idx].mac_addr;
		}
	}
	return NULL;
}

void pfm_kni_tx_arp(const int link_id,
		const pfm_ip_addr_t sender_ip_addr,
		const pfm_ip_addr_t target_ip_addr,
		const unsigned char *target_mac_addr)
{
	char ip_str[STR_IP_ADDR_SIZE+1];
	unsigned char *sender_mac_addr;
	struct rte_ether_hdr *eth_hdr;
	struct rte_arp_hdr *arp_hdr;
	struct rte_mbuf *mbuf;
	int ret;
	unsigned char *pkt;
	int pkt_size;
	int idx;

	for(idx=0; idx < kni_count_g; idx++)
	{
		if (kni_info_list_g[idx].ip_addr == sender_ip_addr)
			break;
	}

	if (idx >= kni_count_g)
	{
		pfm_log_rte_err(PFM_LOG_ERR, 
			"pfm_kni_tx_arp() invoked with invalid "
			"sender_ip_addr=%s",
			pfm_ip2str(sender_ip_addr,ip_str));
		return;
	}
	sender_mac_addr = kni_info_list_g[idx].mac_addr;

	mbuf = rte_pktmbuf_alloc(sys_info_g.mbuf_pool);
	if (NULL == mbuf)
	{
#ifdef TRACE
		char sip_str[STR_IP_ADDR_SIZE+1];
		char tip_str[STR_IP_ADDR_SIZE+1];

		pfm_log_rte_err(PFM_LOG_ERR, 
			"rte_pktmbuf_alloc() failed while sending ARP "
			"senderId=%s, targetIp=%s, pkt droped",
			pfm_ip2str(sender_ip_addr,sip_str),
			pfm_ip2str(target_ip_addr,tip_str));
#endif
		return;
	}

	pkt_size = sizeof(struct rte_ether_hdr) +
			sizeof(struct rte_arp_hdr);
	mbuf->data_len = pkt_size;
	mbuf->pkt_len = pkt_size;
	mbuf->port = link_id;

	pkt = rte_pktmbuf_mtod(mbuf, unsigned char *);

	/* encode Ethernet header */
	eth_hdr = (struct rte_ether_hdr *)pkt;
	memcpy(&eth_hdr->s_addr,sender_mac_addr,MAC_ADDR_SIZE);
	memcpy(&eth_hdr->d_addr,target_mac_addr,MAC_ADDR_SIZE);
	eth_hdr->ether_type = htons(RTE_ETHER_TYPE_ARP);

	/* ecode ARP Header */
	arp_hdr = (struct rte_arp_hdr *)
			(pkt + sizeof(struct rte_ether_hdr));
	// Hardware Type
	arp_hdr->arp_hardware = htons(RTE_ARP_HRD_ETHER);
	
	// Protocol Type
	arp_hdr->arp_protocol = htons(RTE_ETHER_TYPE_IPV4);

	// Hw Address Len
	arp_hdr->arp_hlen = RTE_ETHER_ADDR_LEN;

	// Protocol Addess Len
	arp_hdr->arp_plen = sizeof(uint32_t);

	// OpCode
	arp_hdr->arp_opcode = htons(RTE_ARP_OP_REQUEST);

	// Sender HE Address
	memcpy(&arp_hdr->arp_data.arp_sha,sender_mac_addr,MAC_ADDR_SIZE);

	// Sendr IP Address
	arp_hdr->arp_data.arp_sip = htonl(sender_ip_addr);

	// Target HW Address
	memset(&arp_hdr->arp_data.arp_tha, 0, MAC_ADDR_SIZE);

	// Target IP Address
	arp_hdr->arp_data.arp_tip = htonl(target_ip_addr);

	/* TxLoop Ring */
	ret = rte_ring_enqueue(sys_info_g.tx_ring_ptr,mbuf);
	if (0 != ret)
	{
#ifdef TRACE
		char sip_str[STR_IP_ADDR_SIZE+1];
		char tip_str[STR_IP_ADDR_SIZE+1];

		pfm_log_rte_err(PFM_LOG_ERR, 
			"rte_ring_enqueue_burst() failed while sending ARP "
			"senderId=%s, targetIp=%s, pkt droped",
			pfm_ip2str(sender_ip_addr,sip_str),
			pfm_ip2str(target_ip_addr,tip_str));
#endif
		rte_pktmbuf_free(mbuf);
	}
#ifdef TRACE
	char sip_str[STR_IP_ADDR_SIZE+1];
	char tip_str[STR_IP_ADDR_SIZE+1];

	pfm_trace_msg("Enqueued ARP packets to txLoop"
		"senderId=%s, targetIp=%s ",
		pfm_ip2str(sender_ip_addr,sip_str),
		pfm_ip2str(target_ip_addr,tip_str));
#endif
	return;
}

void pfm_kni_broadcast_arp(	const pfm_ip_addr_t target_ip_addr,
				const unsigned char *target_mac_addr)
{
	int idx;

	for (idx=0; idx < kni_count_g; idx++)
	{
		pfm_kni_tx_arp(
			kni_info_list_g[idx].link_id,
			kni_info_list_g[idx].ip_addr,
			target_ip_addr,
			target_mac_addr);
	}
	return;
}

