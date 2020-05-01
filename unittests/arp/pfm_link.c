#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>


#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_link.h"


#define MAX_LINK_COUNT  5

#define MAX_LINK_NAME_LEN       15

typedef struct {
	uint16_t	link_id;
	char	name[MAX_LINK_NAME_LEN+1];
	uint16_t mtu;
	uint16_t min_mtu;
	uint16_t max_mtu;
	unsigned char   mac_addr[MAC_ADDR_SIZE];
	ops_state_t      ops_state;
} link_info_t;

static int 		link_count_g;
static link_info_t	link_info_list_g[MAX_LINK_COUNT];
static pthread_t	link_mon_thread_id_g = 0;
static const unsigned char default_mac_addr_g[MAC_ADDR_SIZE] =
        { 0x06, 0x0A0, 0xB0, 0xC0, 0xD0, 0xE0 };

/**************************
 *
 * link_statemonitor_pthread_func()
 *
 * function to monitor link operational state.  This function is run
 * as a child pthread. When  state change is detected, it will invoke
 * the call back funtion link_state_change_callback() which need to be 
 * implemeted by appliacion.
 *
 * Args: 
 *	arg	- input the the function. Currently not used.
 *
 *****/

static void *link_statemonitor_pthread_func(__attribute__((unused)) void *arg)
{
	int i;
	ops_state_t curr_state;
	struct rte_eth_link link_info;

        rte_delay_ms(1000);
	pfm_trace_msg( "Link mointoring STARTED");

        while(PFM_TRUE != force_quit_g)
        {
                rte_delay_ms(500); // do it every 0.5 seconds
		
		/* check state of all currently configured links */
		for(i=0; i < link_count_g; i++)
		{
			rte_eth_link_get_nowait(
				link_info_list_g[i].link_id, &link_info);
			if (link_info.link_status == ETH_LINK_UP)
			{
				curr_state = OPSSTATE_ENABLED;
				
			}
			else
			{
				curr_state = OPSSTATE_DISABLED;
			}
			if (curr_state != link_info_list_g[i].ops_state)
			{
				/* ops state change detected for this link
				   invoke callback function */
				link_info_list_g[i].ops_state = curr_state;
				link_state_change_callback(
					link_info_list_g[i].link_id,curr_state);
				pfm_trace_msg(
					"Link '%s(%d)' Ops state chaged to %s",
					link_info_list_g[i].name,
					link_info_list_g[i].link_id,
					((curr_state == OPSSTATE_ENABLED)?
						"ENABLED" : "DISABLED"));
			}
		}
	}
	pfm_trace_msg( "Link mointoring STOPPED");
	return NULL;
}

static pfm_retval_t link_start(int link_id)
{
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf;
	struct rte_eth_txconf tx_conf;
	struct rte_ether_addr addr;
	link_info_t *info_ptr;

        uint16_t rx_sz;
        uint16_t tx_sz;
	uint16_t mtu;

	int idx;
	int ret;
	
	for (idx=0; idx <= link_count_g;idx++)
	{
		if(link_info_list_g[idx].link_id == link_id)
		{
			break;
		}
	}
	if (idx > link_count_g)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"Invalid link_id=%d passed to link_start(). "
			"Need to be less than or equal to link_count=%d.",
			link_id,link_count_g);
		return PFM_FAILED;
	}

	info_ptr = &link_info_list_g[idx];

	ret = rte_eth_dev_is_valid_port(link_id);
	if (1 != ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_is_valid_port(link='%s(%d)') failed. "
			"port is not a valid port.",
			info_ptr->name,
			link_id);
		return PFM_FAILED;
	}
	
	ret = rte_eth_dev_get_mtu(link_id, &mtu);
	if (0 != ret)
	{
		mtu = DEFAULT_MTU_SIZE;
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_get_mtu(link='%s(%d)' failed. "
			"MTU=%d is assumed",
			info_ptr->name,
			link_id);
	}
	info_ptr->mtu = mtu;

	ret = rte_eth_dev_info_get(link_id, &dev_info);
	if (0 != ret)
	{
		
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_info_get(link='%s(%d)') failed. "
			"min_mtu=%d,max_mtu=%d are assumed.",
			info_ptr->name,
			link_id, 
			DEFAULT_MIN_MTU_SIZE,DEFAULT_MAX_MTU_SIZE);
		info_ptr->min_mtu = DEFAULT_MIN_MTU_SIZE;
		info_ptr->max_mtu = DEFAULT_MAX_MTU_SIZE;
	}
	else
	{
		info_ptr->min_mtu = dev_info.min_mtu;
		info_ptr->max_mtu = dev_info.max_mtu;
	}

	/* Read the port MAC address. */
	ret = rte_eth_macaddr_get(link_id, &addr);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_macaddr_get(link='%s(%d)')failed. "
			"A non-zero MAC is assigned automaticaly",
			info_ptr->name,
			link_id);
		/* Assign a non-zero MAC address */
		memcpy(addr.addr_bytes,default_mac_addr_g,MAC_ADDR_SIZE);
		addr.addr_bytes[5] |= link_id;
		ret = rte_eth_dev_default_mac_addr_set(link_id,
		     (struct rte_ether_addr *)info_ptr->mac_addr);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_WARNING,
				"rte_eth_dev_default_mac_addr_set("
				"link='%s(%d)') failed",
				info_ptr->name,
				link_id);
		}
	}

	memcpy(info_ptr->mac_addr,addr.addr_bytes,MAC_ADDR_SIZE);

        memset(&port_conf,0,sizeof(struct rte_eth_conf));
        port_conf.rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
        if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        {
                port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        }
        port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;

        ret = rte_eth_dev_configure(link_id, 1, 1, &port_conf);
        if (ret != 0)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_configure(link='%s(%d)',1,1) failed\n",
			info_ptr->name, link_id);
		return PFM_FAILED;
        }


	rx_sz = RX_RING_SIZE;
	tx_sz = TX_RING_SIZE;
	ret = rte_eth_dev_adjust_nb_rx_tx_desc(link_id, &rx_sz, &tx_sz);
	if (ret != 0)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_adjust_nb_rx_tx_desc('LinkId='%s(%d)',"
			"rx_sz=%d, tx_sz=%d) failed\n",
			info_ptr->name, 
                        link_id,rx_sz,tx_sz);
		return PFM_FAILED;
	}

	ret = rte_eth_rx_queue_setup(link_id, 0, rx_sz,
				rte_eth_dev_socket_id(link_id),
				NULL, sys_info_g.mbuf_pool);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_rx_queue_setup(link='%s(%d)',"
			"0,rx_sz=%d,sockId=%d,NULL,pool=%p) failed",
			info_ptr->name, 
			link_id,rx_sz,rte_eth_dev_socket_id(link_id),
			sys_info_g.mbuf_pool);
		return PFM_FAILED;
	}

	tx_conf = dev_info.default_txconf;
	tx_conf.offloads = port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(	link_id, 0, tx_sz,
                                	rte_eth_dev_socket_id(link_id),
					&tx_conf);
	if (ret < 0)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_tx_queue_setup(link='%s(%d)',0,"
			"tx_sz=%d,sockId=%d,tx_conf=%p",
			info_ptr->name, 
			link_id,tx_sz,
			&tx_conf);
		return PFM_FAILED;
	}
        /* Start the Ethernet port. */
        ret = rte_eth_dev_start(link_id);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
                	"rte_eth_dev_start(link='%s(%d)') failed",
			info_ptr->name, link_id);
		return PFM_FAILED;
        }
#ifdef PROMISCUOUS_MODE

        /* Enable RX in promiscuous mode for the Ethernet device. */
        ret = rte_eth_promiscuous_enable(link_id);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
                	"rte_eth_promiscuous_enable('link=%s(%d)') failed",
			info_ptr->name, link_id);
		return PFM_FAILED;
        }
        pfm_trace_msg("Promiscuous mode enabled for Link='%s(%d)'",
			info_ptr->name, link_id);
#endif

        pfm_trace_msg("Link '%s(%d)' opened successfuly",
			info_ptr->name, link_id);

	pfm_trace_msg("Created link successfully. "
		"link_id=%d,Name=%s,"
		"mtu=%d,min_mtu=%d,max_mtu=%d,"
		"MAC=%02X:%02X:%02X:%02X:%02X:%02X",
		link_id,
		info_ptr->name,
		info_ptr->mtu,
		info_ptr->min_mtu,
		info_ptr->max_mtu,
		info_ptr->mac_addr[0],
		info_ptr->mac_addr[1],
		info_ptr->mac_addr[2],
		info_ptr->mac_addr[3],
		info_ptr->mac_addr[4],
		info_ptr->mac_addr[5]);
	return PFM_SUCCESS;
}

static pfm_retval_t link_stop(int link_id)
{
	int ret;
	int idx;
	char *link_name;

	for (idx=0; idx < link_count_g;idx++)
	{
		if(link_info_list_g[idx].link_id == link_id)
		{
			break;
		}
	}

        if (idx >= link_count_g)
        {
                pfm_log_rte_err(PFM_LOG_ERR,
                        "Invalid link_id=%d passed to link_stop(). "
                        "Need to be less than or equal to link_count=%d.",
                        link_id,link_count_g);
                return PFM_FAILED;
        }

	link_name = link_info_list_g[idx].name;

	/* free mbufs currently cached by the driver */
	ret = rte_eth_tx_done_cleanup(link_id,0,0); //FAIL
	if( 0 > ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_tx_done_cleanup('%s(%d)',0,0) "
			"failed with retval=%d",
			link_name,link_id,ret);
	}

	ret = rte_eth_dev_set_link_down(link_id); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_set_link_down('%s(%d)',0) failed "
			"with retval=%d", 
			link_name,link_id,ret);
	}
/*
 Note: 

     looks like rte_eth_dev_set_link_down() and other functions used below are
     not not supported on VirtualBox VMs. They are failing with (-ENOTSUP)
     return value. In addtion, rte_eth_link_get_nowait() funtion always 
     tell the link is enabled even after it is stoped. Hence the link monitor
     program always detecte it the link as ENABLED. Hence, currently
     the link is not disabled when all associated KNI is disabled.
     This implemenation need to be check on real hardware

     
	ret = rte_eth_dev_tx_queue_stop(link_id,0); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_tx_queue_stop('%s(%d)',0) failed "
			"with retval=%d", 
			link_name,link_id,ret);
	}
	ret = rte_eth_dev_rx_queue_stop(link_id,0); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_rx_queue_stop('%s(%d)',0) failed"
			"with retval=%d", 
			link_name,link_id,ret);
	}
	rte_eth_dev_stop(link_id);

	//rte_eth_dev_close (link_id);
	ret = rte_eth_dev_reset(link_id); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_reset('%s(%d)',0) failed"
			"with retval=%d", 
			link_name,link_id,ret);
	}
********/

	pfm_trace_msg("Link '%s(%d)' closed successfuly.",
			link_name,link_id);

	return PFM_SUCCESS;
}
/****************
 *
 * pfm_link_open()
 * 
 * Open ethernet link with given name.
 *
 * Args:
 *     link_name - input. Name of the link to be oppened
 *
 * Return:
 *	>= 0	- link opened successflly. Return link_id of the oped link. 
 *	< 0	- failed to open link
 *
 *****/
int pfm_link_open(const char *link_name)
{
	uint16_t link_id;

	pfm_retval_t ret_val;
	int ret;
	int i;
	
	for (i=0; i < link_count_g; i++)
	{
		ret = strcmp(link_info_list_g[i].name,link_name);
		if (0 == ret)
		{
			pfm_trace_msg("Link '%s' is already open. "
				"No need to open it again. "
				"Re-using it.",
				link_name);
			return link_info_list_g[i].link_id;
		}
	}

	if (MAX_LINK_COUNT <= link_count_g)
	{
		pfm_trace_msg("Can't open link '%s'. "
				"Max links %d alread opened. ",
				link_name,MAX_LINK_COUNT);
		return (-1);
	}

	ret = rte_eth_dev_get_port_by_name( link_name, &link_id);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_get_port_by_name(link='%s') failed. "
			"link with given name may not exit.",
			link_name);
                return (-2);
        }

	link_info_list_g[link_count_g].link_id = link_id;
	strncpy(link_info_list_g[link_count_g].name,link_name,MAX_LINK_NAME_LEN);
	link_info_list_g[link_count_g].name[MAX_LINK_NAME_LEN]=0;

	ret_val = link_start(link_count_g);
	if (PFM_SUCCESS != ret_val)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"link_start(link='%s') failed. ",
			link_name);
                return (-3);
	}

	if (0 == link_mon_thread_id_g)
	{
		ret = rte_ctrl_thread_create(&link_mon_thread_id_g,
				"Link Monitor thread", NULL,
				link_statemonitor_pthread_func, NULL);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_ERR,
				"rte_ctrl_thread_create(Link) failed");
		}
	}

	link_count_g++;
        return link_id;

}


/****************
 *
 * pfm_link_close()
 * 
 * Close ethernet link with given name which was opned earlier using
 * pfm_link_open()
 *
 * Args:
 *     link_name - input. Name of the link to be closed
 *
 * Return:
 *	None
 *
 *****/
void pfm_link_close(const char *link_name)
{
	int ret;
	pfm_retval_t	ret_val;

	int i;
	for (i=0; i < link_count_g; i++)
	{
		ret = strcmp(link_info_list_g[i].name,link_name);
		if (0 == ret)
		{
			ret_val = link_stop(link_info_list_g[i].link_id);
			if (PFM_SUCCESS != ret_val)
			{
				pfm_log_msg(PFM_LOG_WARNING,
					"link_stop(link_id=%d) faile",
					link_info_list_g[i].link_id);
			}
			return;
		}
	}
	pfm_log_msg(PFM_LOG_WARNING,
               	"Trying to closelink='%s'which is not opened or does not exist."
		"Request discarded",
		link_name);
        return;
}

/************
 *
 * link_read()
 *
 * Read packt bust from one of the open link. Link are checked for 
 * packet bust in round robin method.
 *
 * Args:
 *	rx_pkts		- input/output. Array of pointes were the received
 *			  packtes will be returend 
 *	burst_size	- input. size of rxPkt[] arrary. i.e, how many
 *			  packes can be receved at a time
 *	port		- output. how many packets are reced in teh burst
 *
 * Return:
 *	>0		- Number of packets actually retrieved, which is the
 *                        number of pointers mbuf pointers return in output
 *			  argument rxPkt array.
 *	0		- no packtes recceved on any of the ports
 *	<0		- Error is receving packet burst
 *
 ****/	

int link_read(struct rte_mbuf *rx_pkts[],uint16_t burst_size,uint16_t *port)
{
	static uint16_t next_read = 0;
	int start;
	int rx_sz;

	/* Read links in round robin */
	start = next_read;
	do
	{
		rx_sz = 0;

		/* check the link only if the link is in enabled state*/
		if (OPSSTATE_ENABLED ==
			link_info_list_g[next_read].ops_state)
		{
			rx_sz = rte_eth_rx_burst(
					link_info_list_g[next_read].link_id,
                                        0, rx_pkts, burst_size);

			/* store the link_id in output argument */
			*port = link_info_list_g[next_read].link_id;
		}

		/* Next iteration shold check next link in round robin.*/
		next_read++;
		if (next_read >= link_count_g)
		{
			// wrap around
			next_read = 0;
		}

		/* if a bust is received, retrun the funtion*/
		if (0 < rx_sz)
		{
			return rx_sz;
		}
	} while (next_read != start);

	/* non of the enabled links have packets. hence return 0 */
	return 0;
}


/****************
 *
 * link_state_change()
 *
 * Change operational state of a link.
 *
 * Args:
 *	link_id	- input. Id of the link for which state need to be chaged
 *	disiredState - input. new state
 *
 * Return: None
 *
 ***********/

void link_state_change(const int link_id,const ops_state_t desired_state)
{
	int ret;

/* ISSUE: 
   Below code is not working. Without this code everytime when a KNI
   is enabled, it will reset the link which impact the other KNI which
   need to be avoided. Need more investigation to resolve this issue 

	ops_state_t curr_state;
	struct rte_eth_link link_info;

	rte_eth_link_get_nowait(link_id, &link_info);
	if (link_info.link_status == ETH_LINK_UP)
	{
		curr_state = OPSSTATE_ENABLED;
 	}
	else
	{
		curr_state = OPSSTATE_DISABLED;
	}

	if ( curr_state == desired_state)
	{
		pfm_trace_msg("LinkId=%d state is alreaady in %s. "
			"No change needed",
			link_id, 
		    ((OPSSTATE_DISABLED == desired_state) ? "DOWN" : "UP"));
		return;
	}
*/

	ret = rte_eth_dev_is_valid_port(link_id);
	if (0 == ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR, "Invalid link_id=%d passed",
				link_id);
		return;
	}

	if (OPSSTATE_ENABLED == desired_state)
	{
		rte_eth_dev_stop(link_id);
                rte_eth_dev_start(link_id);
	}
	else
	{
		rte_eth_dev_stop(link_id);
	}
	pfm_trace_msg("OpState of link_id=%d changed to %s.",
			link_id, 
			((OPSSTATE_DISABLED == desired_state) ? "DOWN" : "UP"));

	return;
}

void pfm_link_list_print(FILE *fp)
{
	int idx;

	fprintf(fp,"   %-2s %-12s   %-18s  STATE\n",
			"ID","LINK NAME","MAC ADDRESS");	
	for(idx=0; idx < link_count_g; idx++)
	{
		fprintf(fp,"   %-2d %-12s   "
				"%02X:%02X:%02X:%02X:%02X:%02X   %s\n",
			link_info_list_g[idx].link_id,
			link_info_list_g[idx].name,
			link_info_list_g[idx].mac_addr[0],
			link_info_list_g[idx].mac_addr[1],
			link_info_list_g[idx].mac_addr[2],
			link_info_list_g[idx].mac_addr[3],
			link_info_list_g[idx].mac_addr[4],
			link_info_list_g[idx].mac_addr[5],
			((OPSSTATE_ENABLED==link_info_list_g[idx].ops_state)
				? "ENABLED" : "DISABLED"));
	}
}

void pfm_link_show_print(FILE *fp, int link_id)
{
	int idx;
	for (idx=0; idx < link_count_g; idx++)
	{
		if (link_info_list_g[idx].link_id == link_id)
			break;
	}
	if (idx < link_count_g)
	{
		fprintf(fp,"   %-10s= %d\n", "Link ID",
			link_info_list_g[idx].link_id);
		fprintf(fp,"   %-10s= %s\n", "Name",
			link_info_list_g[idx].name);
		fprintf(fp,"   %-10s= %02X:%02X:%02X:%02X:%02X:%02X\n",
			"MAC Addr",
			link_info_list_g[idx].mac_addr[0],
			link_info_list_g[idx].mac_addr[1],
			link_info_list_g[idx].mac_addr[2],
			link_info_list_g[idx].mac_addr[3],
			link_info_list_g[idx].mac_addr[4],
			link_info_list_g[idx].mac_addr[5]);
		fprintf(fp,"   %-10s= %d\n",
			"MTU",
			link_info_list_g[idx].mtu);
		fprintf(fp,"   %-10s= %d\n",
			"Min MTU",
			link_info_list_g[idx].min_mtu);
		fprintf(fp,"   %-10s= %d\n",
			"Max MTU",
			link_info_list_g[idx].max_mtu);
		fprintf(fp,"   %-10s= %s\n",
			"State",
			((OPSSTATE_ENABLED==link_info_list_g[idx].ops_state)
				? "ENABLED" : "DISABLED"));
	}
	else
	{
		fprintf(fp,"ERROR: Link with ID %d does not exist\n",
			link_id);
	}
}
