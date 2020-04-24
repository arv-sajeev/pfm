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
#include "link.h"
#include "kni.h"


typedef struct 
{
	struct		rte_kni *ptr;
	char		name[RTE_KNI_NAMESIZE+1];
	unsigned char	ipAddr[IP_ADDR_SIZE];
	int		subnetMaskLen;
	int		linkId;
	ops_state_t	opsState;
	admin_state_t	adminState;
} KniInfo_t;

static int KniCount = 0;
static KniInfo_t KniInfoList[MAX_KNI_PORTS];
static const unsigned char defaultMacAddr[MAC_ADDR_SIZE] =
	{ 0x06, 0x10, 0x20, 0x30, 0x40, 0x50 };

/* Callback function which is invoked when MTU is changed*/
static int kniChangeMtu(uint16_t kniId, unsigned int newMtu)
{
	pfm_trace_msg("kniChangeMtu(kniId=%d, newMtu=%d) invoked. "
				"But MTU change is not yet implemented",
				kniId,newMtu);
	
       
	return 0;
}

static pfm_retval_t  ipAddrConfig(	const char *ifName,
			ops_state_t kniOpsState,
			const unsigned char *ipAddr,
			const int maskLen)
{
	static char strIpAddr[30];
	char *operation;
	pid_t pid; 
	int status; 

	/* Convert IP address to string */	
	sprintf(strIpAddr,"%d.%d.%d.%d/%d",
		ipAddr[0],ipAddr[1],ipAddr[2],ipAddr[3],maskLen);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
	if (OPSSTATE_ENABLED == kniOpsState)
	{
		operation = "add";
	}
	else
	{
		operation = "del";
	}
#pragma GCC diagnostic pop

	/* start a chiled process */
	pid = fork(); 
	if ((-1) == pid )
	{
		/* fork() failed */
		pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"fork() failed",strIpAddr,ifName);
		return PFM_FAILED;
	}
	pfm_trace_msg("Child prcess fork() sucessful"); 
	if ( 0 == pid )
	{
		/* in the context of child process */	
		/* CMD format is "ip addr add 192.168.121.45/24 dev eth0" */

		const char *const argList[] = 
		   { "ip", "addr", operation, strIpAddr, "dev", ifName, NULL };

		pfm_trace_msg("Invoking execvp(%s %s %s %s %s %s)",
			argList[0],argList[1],argList[2],
			argList[3],argList[4],argList[5]); 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
		execvp(argList[0], argList); 
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
				"WIFEXITED() returm failure",strIpAddr,ifName);
				return PFM_FAILED;
			}
			else
			{
				pfm_trace_msg("IP %s set to KNI %s",
						strIpAddr,ifName);
				return PFM_SUCCESS;
			}
		}	
		else
		{
			pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"Child did not teminate normaly\n",
				strIpAddr,ifName);
			return PFM_FAILED;
		}
	} 
	else
	{ 
		// waitpid() failed 
		pfm_log_std_err(PFM_LOG_ERR,
				"Not able to assign IP %s to KNI %s. "
				"waitpid() failed",
				strIpAddr,ifName);
		return PFM_FAILED;
	} 
}

/* Callback function which is invoked when configuring each KNI interface */
static int kniConfigNetworkIf(uint16_t kniId, uint8_t kniState)
{
	int linkId;
	ops_state_t newOpsState;
	int idx;

	if (kniId >= KniCount)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Invalid KniId=%d passed to kniConfigNetworkIf()",
			kniId);
		return 0;
	}

	if ( NULL == KniInfoList[kniId].ptr) 
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"KNI ptr for kniId=%d is null",kniId);
		return 0;
	}

	pfm_trace_msg("Kni %s(%d) state changed to %s",
			KniInfoList[kniId].name,
			kniId,
			((ETH_LINK_UP == kniState) ? "UP": "DOWN"));

	if (ETH_LINK_UP == kniState)
	{
		newOpsState = OPSSTATE_ENABLED;
		KniInfoList[kniId].opsState = OPSSTATE_ENABLED;
	}
	else
	{
		newOpsState = OPSSTATE_DISABLED;
		KniInfoList[kniId].opsState = OPSSTATE_DISABLED;
	}
	linkId = KniInfoList[kniId].linkId;

	if (OPSSTATE_ENABLED == newOpsState )
	{
		pfm_trace_msg("At least one KNI for LinkId=%d is UP. "
			"Hence, Enable the Link",linkId);
		/* if any of the kni associated with link is enabled
		   then the link needs to be enabled */
		LinkStateChange(linkId,OPSSTATE_ENABLED);
	}
	else
	{
		/* if all the knis associated with alink is in diabled state
		   the the link can be dissabled */
		for(idx=0; idx < KniCount; idx++)
		{
			if ( KniInfoList[idx].linkId == linkId) 
			{
				if(KniInfoList[idx].opsState !=
							OPSSTATE_DISABLED)
				{
					break;
				}
			}
		}
		if (idx < KniCount)
		{
			/* atlest one KNI which is mapped to the Link is
			   still in enabled state. Hence the link
			   can not be disabled */
			pfm_trace_msg("At least one KNI of LinkId=%d is UP."
				" Hence, can not disable Link",linkId);
		}
		else
		{
			/* all KNIs of linka are in disabe state.
			   hence disable the link */
			pfm_trace_msg("All KNIs for LinkId=%d are DOWN. "
				"Hence, disable Link",linkId);
			LinkStateChange(linkId,OPSSTATE_DISABLED);
		}
	}
	pfm_trace_msg("KNI '%s' state changed to %s",
			KniInfoList[kniId].name,
			((newOpsState == OPSSTATE_ENABLED) ?
				"UP":"DOWN"));
	return 0;
}


static void kniSetState(const KniInfo_t *kni_ptr, const ops_state_t desiredState)
{
	int prevState, newState;

	if (NULL == kni_ptr->ptr)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"KNI ptr for KNI=%s is null",kni_ptr->name);
		return;
	}
	newState = ETH_LINK_DOWN;
	if (OPSSTATE_ENABLED == desiredState)
	{
		newState = ETH_LINK_UP;
	}

	prevState = rte_kni_update_link(kni_ptr->ptr,newState);
	if (0 > prevState)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"Not able to set Kni state. "
			"rte_kni_update_link(kni=%s,newState=%s) failed"
			" with retVal=%d",
			kni_ptr->name,
			((newState == ETH_LINK_UP) ? "UP":"DOWN"),
			prevState);
		return;
	}

	if (prevState == newState)
	{
		pfm_trace_msg("KNI '%s' state is not changed. "
			"It is still %s.",
			kni_ptr->name,
			(newState == ETH_LINK_UP) ? "UP":"DOWN");
		return;
	}
	pfm_trace_msg("KNI '%s' state is changed to %s.",
			kni_ptr->name,
			(newState == ETH_LINK_UP) ? "UP":"DOWN");
	return;
}

void KniStateChange(const int linkId,const ops_state_t desiredState)
{
	int idx;
	static pfm_bool_t firstCall = PFM_TRUE;
	pfm_retval_t retVal;

	for(idx=0; idx < KniCount; idx++)
	{
		if ( (KniInfoList[idx].linkId == linkId) &&
			(KniInfoList[idx].ptr != NULL))
		{
			kniSetState(&KniInfoList[idx],desiredState);
		}
	}

	if (PFM_TRUE != firstCall)
	{
		return;
	}

	firstCall = PFM_FALSE;

	for(idx=0; idx < KniCount; idx++)
	{
		retVal = ipAddrConfig(KniInfoList[idx].name,
				KniInfoList[idx].opsState,
				KniInfoList[idx].ipAddr, 
				KniInfoList[idx].subnetMaskLen);
		if (PFM_SUCCESS != retVal)
		{
			pfm_log_msg(PFM_LOG_ERR,
				"Failed to %s IP Addr to KNI=%s",
				((KniInfoList[idx].opsState == OPSSTATE_ENABLED) ?
					"ADD" : "REMOVE"),
				KniInfoList[idx].name);
		}
	}

	return;
}
struct rte_kni *KniOpen(const int linkId,
			const char *kniName,
			const unsigned char *ipAddr,
			const int subnetMaskLen)
{
	struct rte_kni *kni_ptr;
	struct rte_kni_conf conf;
	struct rte_kni_ops ops;
	struct rte_eth_dev_info dev_info;
	int retVal;
	int ret;
	int idx;
        
	/* Initialize KNI subsystem if it is the 1st port*/
	if (0 == KniCount)
	{
		retVal = rte_kni_init(MAX_KNI_PORTS);
		if (0 != retVal)
		{
			pfm_log_rte_err(PFM_LOG_EMERG,
				"rte_kni_init(%d) failed",MAX_KNI_PORTS);
			return NULL;
		}
		pfm_trace_msg("Initialized KNI subsystem");
	}

	for(idx=0; idx < KniCount; idx++)
	{
		if (0 == strcmp(KniInfoList[idx].name,kniName))
		{
			pfm_log_msg(PFM_LOG_WARNING,
				"KNI with name %s already exists. "
				"Hence not created again",
				kniName);
			return NULL;
		}
	}

	if (KniCount >= MAX_KNI_PORTS)
	{
		pfm_log_msg(PFM_LOG_ERR,
			"Trying to open too many (%d) KNI ports. "
			"Only %d is allowed. Request failed.",
			(KniCount+1),MAX_KNI_PORTS);
		return NULL;
	}


	memset(&conf, 0, sizeof(conf));
	strncpy(conf.name,kniName,RTE_KNI_NAMESIZE);
	conf.core_id = LCORE_TXLOOP;
	conf.group_id = linkId;
	conf.mbuf_size = RTE_MBUF_DEFAULT_DATAROOM;
	// conf.addr; Not filled
	// conf.id; Not filled
	// conf.force_bind; Not filled

	/* get MAC addres of the associated link */
	ret = rte_eth_macaddr_get(linkId,
                        (struct rte_ether_addr *)&conf.mac_addr);
        if (ret != 0)
        {
		memcpy(conf.mac_addr,defaultMacAddr,MAC_ADDR_SIZE);
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Not able to get MAC address for link %d. "
			"rte_eth_macaddr_get() failed. "
			"Assumed default MACAddr = "
			"%02X:%02X:%02X:%02X:%02X:%02X",
			linkId,
			conf.mac_addr[0],conf.mac_addr[1],
			conf.mac_addr[2],conf.mac_addr[3],
			conf.mac_addr[4],conf.mac_addr[5]);
        }

	/* Get MTU size of the associated link */
        ret = rte_eth_dev_get_mtu(linkId, &conf.mtu);
        if (ret != 0)
        {
        	conf.mtu = DEFAULT_MTU_SIZE;
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Not able to get MTU Size for link %d. "
			"rte_eth_dev_get_mtu() failed. "
			"Assumed default value MTU =  %d",
			linkId, conf.mtu);
        }

	/* Get min and max MTU size supported by associated link */
	ret = rte_eth_dev_info_get(linkId, &dev_info);
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
			linkId, conf.min_mtu,conf.max_mtu);
	}

	memset(&ops, 0, sizeof(ops));
	ops.port_id = KniCount;
	ops.change_mtu = kniChangeMtu;
	ops.config_network_if = kniConfigNetworkIf;
	ops.config_mac_address = NULL;
    	ops.config_promiscusity = NULL;
   	ops.config_allmulticast = NULL;
	kni_ptr = rte_kni_alloc(sys_info_g.mbuf_pool, &conf, &ops);
	if (NULL == kni_ptr)
	{
		pfm_log_rte_err(PFM_LOG_ERR,
			"rte_kni_alloc() failed", retVal);
		return NULL;
	}
	pfm_trace_msg("Allocated KNI interface '%s', LinkId=%d."
			"with MAC=%02X:%02X:%02X:%02X:%02X:%02X,"
			"MTU=%d,minMTU=%d,maxMTU=%d",
			kniName,linkId,
			conf.mac_addr[0],conf.mac_addr[1],
			conf.mac_addr[2],conf.mac_addr[3],
			conf.mac_addr[4],conf.mac_addr[5],
			conf.mtu,
			conf.min_mtu,
			conf.max_mtu);


	KniInfoList[KniCount].ptr = kni_ptr;
	strncpy(KniInfoList[KniCount].name,kniName,RTE_KNI_NAMESIZE);
	memcpy(KniInfoList[KniCount].ipAddr,ipAddr,IP_ADDR_SIZE);
	KniInfoList[KniCount].subnetMaskLen = subnetMaskLen;
	KniInfoList[KniCount].linkId = linkId;
	KniCount++;
	return kni_ptr;
}

void KniClose(struct rte_kni *kni)
{
	int ret;
	int idx;

	if (NULL == kni)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"NULL KNI pointer passed to KniClose()");
		return;
	}

	for (idx=0; idx < KniCount; idx++)
	{
		if (KniInfoList[idx].ptr == kni)
		{
			break;
		}
	}
	if (idx >= KniCount)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Unknow KNI pointer %p passed to KniClose()",kni);
		return;
	}
	KniInfoList[idx].ptr = NULL;
	ret = rte_kni_release(kni);
	if (0 != ret)
	{
		pfm_log_rte_err(PFM_LOG_WARNING,"rte_kni_release() failed");
		return;
	}
	pfm_trace_msg("Released KNI interface '%s(%d)'",
			KniInfoList[idx].name, idx);
	return;
}


/********
 * Write a burst of packets to KNI interface
 *
 * Args:
 *	kni	-	INPUT		- pointer to KNI where the packet
 *					  to be send.
 *	pktBurst-	INPUT		- array of packet to be send
 *	numPkts	-	INPUT		- number of packes in 'pktBurst'
 *					  
 *
 * Return: Number of packets actuly written
 ******/
void KniWrite(	struct rte_kni *kni,
			struct rte_mbuf *pktBurst[],
			const unsigned int numPkts)
{
	unsigned int pktsSend;

	if (NULL == kni)
	{
		/* KNI is not yet openned */
		return;
	}

	/* Burst tx to kni */
	pktsSend = rte_kni_tx_burst(kni, pktBurst, numPkts);
	if (pktsSend < numPkts)
	{
		/* some packets are not send.
		   Drop them by releasing the buffers */
		pfm_log_rte_err(PFM_LOG_WARNING,
			"Dropped %d slow path packets rte_kni_tx_burst()",
			(numPkts-pktsSend));
		for(;pktsSend < numPkts; pktsSend++)
		{
			rte_pktmbuf_free(pktBurst[pktsSend]);
		}
	}
	rte_kni_handle_request(kni);
	
	return;
}


/********
 * Read a burst of packets from KNI interface
 *
 * Args:
 *	pktBurst-	input/output. read packets are stored in this array
 *	kni	-	input- pointer to KNI which need to be read
 *	numPkts	-	INPUT		- size of 'pktBurst'. i.e.
 *					  i.e, max packet to read.
 *
 * Return: Number of packets actuly read
 *
 ******/
int KniRead(	struct rte_mbuf *pktBurst[],
		uint16_t burstSize,
		int *linkId)
{
	static uint16_t kniIdxToReadNext = 0;
        int kniIdxStart;
	int pktsRecvd;

	kniIdxStart = kniIdxToReadNext;
	do
	{
		pktsRecvd = 0;
		if (NULL != KniInfoList[kniIdxToReadNext].ptr)
		{
			rte_kni_handle_request(
				KniInfoList[kniIdxToReadNext].ptr);

			/* check only if the kni is in unlocked state*/
	                if (OPSSTATE_ENABLED ==
	                        KniInfoList[kniIdxToReadNext].opsState)
	                {
				if (KniInfoList[kniIdxToReadNext].ptr != NULL)
				{
					pktsRecvd = rte_kni_rx_burst(
					     KniInfoList[kniIdxToReadNext].ptr,
						pktBurst,
						burstSize);

                     		  	/* store the kni name in output
					   argument */
					*linkId =
					   KniInfoList[kniIdxToReadNext].linkId;
				};
			};
		}

                /* Next iteration shold check next KNI in round robin.*/
                kniIdxToReadNext++;
                if (kniIdxToReadNext >= KniCount)
                {
                        // wrap around
                        kniIdxToReadNext = 0;
                }

                /* if a bust is received, retrun the funtion*/
                if (0 < pktsRecvd)
                {
			//printf("KNI Tx. %d Pkts on Link=%d\n",pktsRecvd,*linkId);
                        return pktsRecvd;
                }

	} while (kniIdxToReadNext != kniIdxStart); 

	/* non of the enabled knis have packets. hence return 0 */

	return 0;
}

