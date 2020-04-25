#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_ethdev.h>


#include "pfm.h"
#include "pfm_comm.h"
#include "link.h"


#define MAX_LINK_COUNT  5

#define MAX_LINK_NAME_LEN       15

typedef struct {
	uint16_t	linkId;
	char	name[MAX_LINK_NAME_LEN+1];
	uint16_t mtu;
	uint16_t minMtu;
	uint16_t maxMtu;
	unsigned char   macAddr[MAC_ADDR_SIZE];
	ops_state_t      opsState;
} LinkInfo_t;

static int 		LinkCount;
static LinkInfo_t	LinkInfoList[MAX_LINK_COUNT];
static pthread_t	LinkMonThreadId = 0;
static const unsigned char defaultMacAddr[MAC_ADDR_SIZE] =
        { 0x06, 0x0A0, 0xB0, 0xC0, 0xD0, 0xE0 };

/**************************
 *
 * linkStateMonitorPThreadFunc()
 *
 * function to monitor link operational state.  This function is run
 * as a child pthread. When  state change is detected, it will invoke
 * the call back funtion link_state_change_call_back() which need to be 
 * implemeted by appliacion.
 *
 * Args: 
 *	arg	- input the the function. Currently not used.
 *
 *****/

static void *linkStateMonitorPThreadFunc(__attribute__((unused)) void *arg)
{
	int i;
	ops_state_t currState;
	struct rte_eth_link linkInfo;

        rte_delay_ms(1000);
	pfm_trace_msg( "Link mointoring STARTED");

        while(PFM_TRUE != force_quit_g)
        {
                rte_delay_ms(500); // do it every 0.5 seconds
		
		/* check state of all currently configured links */
		for(i=0; i < LinkCount; i++)
		{
			rte_eth_link_get_nowait(
				LinkInfoList[i].linkId, &linkInfo);
			if (linkInfo.link_status == ETH_LINK_UP)
			{
				currState = OPSSTATE_ENABLED;
				
			}
			else
			{
				currState = OPSSTATE_DISABLED;
			}
			if (currState != LinkInfoList[i].opsState)
			{
				/* ops state change detected for this link
				   invoke callback function */
				LinkInfoList[i].opsState = currState;
				link_state_change_call_back(
					LinkInfoList[i].linkId,currState);
				pfm_trace_msg(
					"Link '%s(%d)' Ops state chaged to %s",
					LinkInfoList[i].name,
					LinkInfoList[i].linkId,
					((currState == OPSSTATE_ENABLED)?
						"ENABLED" : "DISABLED"));
			}
		}
	}
	pfm_trace_msg( "Link mointoring STOPPED");
	return NULL;
}

static pfm_retval_t linkStart(int linkId)
{
	struct rte_eth_dev_info devInfo;
	struct rte_eth_conf portConf;
	struct rte_eth_txconf txConf;
	struct rte_ether_addr addr;
	LinkInfo_t *linkInfoPtr;

        uint16_t nbRxd;
        uint16_t nbTxd;
	uint16_t mtu;

	int idx;
	int ret;
	
	for (idx=0; idx <= LinkCount;idx++)
	{
		if(LinkInfoList[idx].linkId == linkId)
		{
			break;
		}
	}
	if (idx > LinkCount)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"Invalid linkId=%d passed to linkStart(). "
			"Need to be less than or equal to LinkCount=%d.",
			linkId,LinkCount);
		return PFM_FAILED;
	}

	linkInfoPtr = &LinkInfoList[idx];

	ret = rte_eth_dev_is_valid_port(linkId);
	if (1 != ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_is_valid_port(link='%s(%d)') failed. "
			"port is not a valid port.",
			linkInfoPtr->name,
			linkId);
		return PFM_FAILED;
	}
	
	ret = rte_eth_dev_get_mtu(linkId, &mtu);
	if (0 != ret)
	{
		mtu = DEFAULT_MTU_SIZE;
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_get_mtu(link='%s(%d)' failed. "
			"MTU=%d is assumed",
			linkInfoPtr->name,
			linkId);
	}
	linkInfoPtr->mtu = mtu;

	ret = rte_eth_dev_info_get(linkId, &devInfo);
	if (0 != ret)
	{
		
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_info_get(link='%s(%d)') failed. "
			"minMtu=%d,maxMtu=%d are assumed.",
			linkInfoPtr->name,
			linkId, 
			DEFAULT_MIN_MTU_SIZE,DEFAULT_MAX_MTU_SIZE);
		linkInfoPtr->minMtu = DEFAULT_MIN_MTU_SIZE;
		linkInfoPtr->maxMtu = DEFAULT_MAX_MTU_SIZE;
	}
	else
	{
		linkInfoPtr->minMtu = devInfo.min_mtu;
		linkInfoPtr->maxMtu = devInfo.max_mtu;
	}

	/* Read the port MAC address. */
	ret = rte_eth_macaddr_get(linkId, &addr);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_macaddr_get(link='%s(%d)')failed. "
			"A non-zero MAC is assigned automaticaly",
			linkInfoPtr->name,
			linkId);
		/* Assign a non-zero MAC address */
		memcpy(addr.addr_bytes,defaultMacAddr,MAC_ADDR_SIZE);
		addr.addr_bytes[5] |= linkId;
		ret = rte_eth_dev_default_mac_addr_set(linkId,
		     (struct rte_ether_addr *)linkInfoPtr->macAddr);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_WARNING,
				"rte_eth_dev_default_mac_addr_set("
				"link='%s(%d)') failed",
				linkInfoPtr->name,
				linkId);
		}
	}

	memcpy(linkInfoPtr->macAddr,addr.addr_bytes,MAC_ADDR_SIZE);

        memset(&portConf,0,sizeof(struct rte_eth_conf));
        portConf.rxmode.max_rx_pkt_len = RTE_ETHER_MAX_LEN;
        if (devInfo.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        {
                portConf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
        }
        portConf.rx_adv_conf.rss_conf.rss_hf &= devInfo.flow_type_rss_offloads;

        ret = rte_eth_dev_configure(linkId, 1, 1, &portConf);
        if (ret != 0)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_configure(link='%s(%d)',1,1) failed\n",
			linkInfoPtr->name, linkId);
		return PFM_FAILED;
        }


	nbRxd = RX_RING_SIZE;
	nbTxd = TX_RING_SIZE;
	ret = rte_eth_dev_adjust_nb_rx_tx_desc(linkId, &nbRxd, &nbTxd);
	if (ret != 0)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_adjust_nb_rx_tx_desc('LinkId='%s(%d)',"
			"nbRxd=%d, nbTxd=%d) failed\n",
			linkInfoPtr->name, 
                        linkId,nbRxd,nbTxd);
		return PFM_FAILED;
	}

	ret = rte_eth_rx_queue_setup(linkId, 0, nbRxd,
				rte_eth_dev_socket_id(linkId),
				NULL, sys_info_g.mbuf_pool);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_rx_queue_setup(link='%s(%d)',"
			"0,nbRxd=%d,sockId=%d,NULL,pool=%p) failed",
			linkInfoPtr->name, 
			linkId,nbRxd,rte_eth_dev_socket_id(linkId),
			sys_info_g.mbuf_pool);
		return PFM_FAILED;
	}

	txConf = devInfo.default_txconf;
	txConf.offloads = portConf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(	linkId, 0, nbTxd,
                                	rte_eth_dev_socket_id(linkId),
					&txConf);
	if (ret < 0)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_tx_queue_setup(link='%s(%d)',0,"
			"nbTxd=%d,sockId=%d,txConf=%p",
			linkInfoPtr->name, 
			linkId,nbTxd,
			&txConf);
		return PFM_FAILED;
	}
        /* Start the Ethernet port. */
        ret = rte_eth_dev_start(linkId);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
                	"rte_eth_dev_start(link='%s(%d)') failed",
			linkInfoPtr->name, linkId);
		return PFM_FAILED;
        }
#ifdef PROMISCUOUS_MODE

        /* Enable RX in promiscuous mode for the Ethernet device. */
        ret = rte_eth_promiscuous_enable(linkId);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
                	"rte_eth_promiscuous_enable('link=%s(%d)') failed",
			linkInfoPtr->name, linkId);
		return PFM_FAILED;
        }
        pfm_trace_msg("Promiscuous mode enabled for Link='%s(%d)'",
			linkInfoPtr->name, linkId);
#endif

        pfm_trace_msg("Link '%s(%d)' opened successfuly",
			linkInfoPtr->name, linkId);

	pfm_trace_msg("Created link successfully. "
		"linkId=%d,Name=%s,"
		"mtu=%d,minMtu=%d,maxMtu=%d,"
		"MAC=%02X:%02X:%02X:%02X:%02X:%02X",
		linkId,
		linkInfoPtr->name,
		linkInfoPtr->mtu,
		linkInfoPtr->minMtu,
		linkInfoPtr->maxMtu,
		linkInfoPtr->macAddr[0],
		linkInfoPtr->macAddr[1],
		linkInfoPtr->macAddr[2],
		linkInfoPtr->macAddr[3],
		linkInfoPtr->macAddr[4],
		linkInfoPtr->macAddr[5]);
	return PFM_SUCCESS;
}

static pfm_retval_t linkStop(int linkId)
{
	int ret;
	int idx;
	char *linkName;

	for (idx=0; idx < LinkCount;idx++)
	{
		if(LinkInfoList[idx].linkId == linkId)
		{
			break;
		}
	}

        if (idx >= LinkCount)
        {
                pfm_log_rte_err(PFM_LOG_ERR,
                        "Invalid linkId=%d passed to linkStop(). "
                        "Need to be less than or equal to LinkCount=%d.",
                        linkId,LinkCount);
                return PFM_FAILED;
        }

	linkName = LinkInfoList[idx].name;

	/* free mbufs currently cached by the driver */
	ret = rte_eth_tx_done_cleanup(linkId,0,0); //FAIL
	if( 0 > ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_tx_done_cleanup('%s(%d)',0,0) "
			"failed with retval=%d",
			linkName,linkId,ret);
	}

	ret = rte_eth_dev_set_link_down(linkId); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_set_link_down('%s(%d)',0) failed "
			"with retval=%d", 
			linkName,linkId,ret);
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

     
	ret = rte_eth_dev_tx_queue_stop(linkId,0); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_tx_queue_stop('%s(%d)',0) failed "
			"with retval=%d", 
			linkName,linkId,ret);
	}
	ret = rte_eth_dev_rx_queue_stop(linkId,0); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_rx_queue_stop('%s(%d)',0) failed"
			"with retval=%d", 
			linkName,linkId,ret);
	}
	rte_eth_dev_stop(linkId);

	//rte_eth_dev_close (linkId);
	ret = rte_eth_dev_reset(linkId); 
	if( 0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"rte_eth_dev_reset('%s(%d)',0) failed"
			"with retval=%d", 
			linkName,linkId,ret);
	}
********/

	pfm_trace_msg("Link '%s(%d)' closed successfuly.",
			linkName,linkId);

	return PFM_SUCCESS;
}
/****************
 *
 * pfm_link_open()
 * 
 * Open ethernet link with given name.
 *
 * Args:
 *     linkName - input. Name of the link to be oppened
 *
 * Return:
 *	>= 0	- link opened successflly. Return linkId of the oped link. 
 *	< 0	- failed to open link
 *
 *****/
int pfm_link_open(const char *linkName)
{
	uint16_t linkId;

	pfm_retval_t retVal;
	int ret;
	int i;
	
	for (i=0; i < LinkCount; i++)
	{
		ret = strcmp(LinkInfoList[i].name,linkName);
		if (0 == ret)
		{
			pfm_trace_msg("Link '%s' is already open. "
				"No need to open it again. "
				"Re-using it.",
				linkName);
			return LinkInfoList[i].linkId;
		}
	}

	if (MAX_LINK_COUNT <= LinkCount)
	{
		pfm_trace_msg("Can't open link '%s'. "
				"Max links %d alread opened. ",
				linkName,MAX_LINK_COUNT);
		return (-1);
	}

	ret = rte_eth_dev_get_port_by_name( linkName, &linkId);
        if (0 != ret)
        {
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_eth_dev_get_port_by_name(link='%s') failed. "
			"link with given name may not exit.",
			linkName);
                return (-2);
        }

	LinkInfoList[LinkCount].linkId = linkId;
	strncpy(LinkInfoList[LinkCount].name,linkName,MAX_LINK_NAME_LEN);
	LinkInfoList[LinkCount].name[MAX_LINK_NAME_LEN]=0;

	retVal = linkStart(LinkCount);
	if (PFM_SUCCESS != retVal)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"linkStart(link='%s') failed. ",
			linkName);
                return (-3);
	}

	if (0 == LinkMonThreadId)
	{
		ret = rte_ctrl_thread_create(&LinkMonThreadId,
				"Link Monitor thread", NULL,
				linkStateMonitorPThreadFunc, NULL);
		if (0 != ret)
		{
			pfm_log_rte_err(PFM_LOG_ERR,
				"rte_ctrl_thread_create(Link) failed");
		}
	}

	LinkCount++;
        return linkId;

}


/****************
 *
 * pfm_link_close()
 * 
 * Close ethernet link with given name which was opned earlier using
 * pfm_link_open()
 *
 * Args:
 *     linkName - input. Name of the link to be closed
 *
 * Return:
 *	None
 *
 *****/
void pfm_link_close(const char *linkName)
{
	int ret;
	pfm_retval_t	retVal;

	int i;
	for (i=0; i < LinkCount; i++)
	{
		ret = strcmp(LinkInfoList[i].name,linkName);
		if (0 == ret)
		{
			retVal = linkStop(LinkInfoList[i].linkId);
			if (PFM_SUCCESS != retVal)
			{
				pfm_log_msg(PFM_LOG_WARNING,
					"linkStop(linkId=%d) faile",
					LinkInfoList[i].linkId);
			}
			return;
		}
	}
	pfm_log_msg(PFM_LOG_WARNING,
               	"Trying to closelink='%s'which is not opened or does not exist."
		"Request discarded",
		linkName);
        return;
}

/************
 *
 * LinkRead()
 *
 * Read packt bust from one of the open link. Link are checked for 
 * packet bust in round robin method.
 *
 * Args:
 *	rxPkt		- input/output. Array of pointes were the received
 *			  packtes will be returend 
 *	burstSize	- input. size of rxPkt[] arrary. i.e, how many
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

int LinkRead(struct rte_mbuf *rxPkts[],uint16_t burstSize,uint16_t *port)
{
	static uint16_t linkIdxToReadNext = 0;
	int linkIdxStart;
	int nbRx;

	/* Read links in round robin */
	linkIdxStart = linkIdxToReadNext;
	do
	{
		nbRx = 0;

		/* check the link only if the link is in enabled state*/
		if (OPSSTATE_ENABLED ==
			LinkInfoList[linkIdxToReadNext].opsState)
		{
			nbRx = rte_eth_rx_burst(
					LinkInfoList[linkIdxToReadNext].linkId,
                                        0, rxPkts, burstSize);

			/* store the linkId in output argument */
			*port = LinkInfoList[linkIdxToReadNext].linkId;
		}

		/* Next iteration shold check next link in round robin.*/
		linkIdxToReadNext++;
		if (linkIdxToReadNext >= LinkCount)
		{
			// wrap around
			linkIdxToReadNext = 0;
		}

		/* if a bust is received, retrun the funtion*/
		if (0 < nbRx)
		{
			return nbRx;
		}
	} while (linkIdxToReadNext != linkIdxStart);

	/* non of the enabled links have packets. hence return 0 */
	return 0;
}


/****************
 *
 * LinkStateChange()
 *
 * Change operational state of a link.
 *
 * Args:
 *	linkId	- input. Id of the link for which state need to be chaged
 *	disiredState - input. new state
 *
 * Return: None
 *
 ***********/

void LinkStateChange(const int linkId,const ops_state_t desiredState)
{
	int ret;

/* ISSUE: 
   Below code is not working. Without this code everytime when a KNI
   is enabled, it will reset the link which impact the other KNI which
   need to be avoided. Need more investigation to resolve this issue 

	ops_state_t currState;
	struct rte_eth_link linkInfo;

	rte_eth_link_get_nowait(linkId, &linkInfo);
	if (linkInfo.link_status == ETH_LINK_UP)
	{
		currState = OPSSTATE_ENABLED;
 	}
	else
	{
		currState = OPSSTATE_DISABLED;
	}

	if ( currState == desiredState)
	{
		pfm_trace_msg("LinkId=%d state is alreaady in %s. "
			"No change needed",
			linkId, 
		    ((OPSSTATE_DISABLED == desiredState) ? "DOWN" : "UP"));
		return;
	}
*/

	ret = rte_eth_dev_is_valid_port(linkId);
	if (0 == ret)
	{
		pfm_log_rte_err(PFM_LOG_ERR, "Invalid linkId=%d passed",
				linkId);
		return;
	}

	if (OPSSTATE_ENABLED == desiredState)
	{
		rte_eth_dev_stop(linkId);
                rte_eth_dev_start(linkId);
	}
	else
	{
		rte_eth_dev_stop(linkId);
	}
	pfm_trace_msg("OpState of linkId=%d changed to %s.",
			linkId, 
			((OPSSTATE_DISABLED == desiredState) ? "DOWN" : "UP"));

	return;
}

