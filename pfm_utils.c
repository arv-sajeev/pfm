#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm_utils.h"

ipv4_addr_t pfm_str2ip(const char *ip_str)
{
	ipv4_addr_t ipn;	// IP in network order
	ipv4_addr_t iph;	// IP in host order
	int ret;

	ret = inet_pton(AF_INET,ip_str,&ipn);
	if (1 != ret)
	{
		pfm_log_std_err(PFM_LOG_WARNING,
				"Failed to convert IP=[%s] to "
				"pfm_ip_addr_t format", ip_str);
		return 0;
	}
	iph = ntohl(ipn);
	return iph;
}

const char *pfm_ip2str(ipv4_addr_t iph,char *ip_str)
{
	ipv4_addr_t ipn;	// IP in network order

	ipn = htonl(iph);
	const char *ret = inet_ntop(AF_INET,&ipn,ip_str,STR_IP_ADDR_SIZE);
	if (NULL == ret)
	{
		pfm_log_std_err(PFM_LOG_ERR,
			"Failed to IP=[0x%0X] to string format",iph);
		return NULL;
	}
	return ip_str;
}

