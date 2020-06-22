#ifndef __GTP_H__
#define __GTP_H__ 1

#include "pfm.h"
#include <rte_timer.h>

typedef struct
{
	pfm_ip_addr_t 	remote_ip;
	uint16_t	remote_port_no;
	pfm_ip_addr_t	local_ip;
	uint16_t	local_port_no;
	uint16_t	tunnel_count;
	uint16_t	retry_count;
} 
gtp_path_info_t;

typedef struct
{
	gtp_path_info_t gtp_path;
	struct rte_timer echo_timer;
}
gtp_table_entry_t;

#endif
