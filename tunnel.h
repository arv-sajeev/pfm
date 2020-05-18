#ifndef __TUNNEL_H__
#define __TUNNEL_H__ 1
#include "pfm.h"

typedef enum
{
	TUNNEL_TYPE_PDUS,
	TUNNEL_TYPE_DRB,
} tunnel_type_t;

typedef struct
{
	unsigned int	drb_id:5;
	unsigned int	is_default:1;
	unsigned int	is_dl_sdap_hdr_enabled:1;
	unsigned int	is_ul_sdap_hdr_enabled:1;
	uint32_t	mapped_flow_idx; // valid only if is_ul_sdap_hdr_enabled
	uint32_t	mapped_pdus_idx; 
} drb_info_t;

typedef struct 
{
		unsigned int flow_id:6;
		unsigned int r_qos_status:2;
		uint32_t mapped_drb_idx;
} flow_info_t;

typedef struct
{
	unsigned int	pdus_id:8;
	unsigned int 	flow_count:6;
	uint64_t ul_new_flow_detected_bit_map;
	flow_info_t flow_list[MAX_FLOWS_PER_PDUS];
} pdus_info_t;

typedef struct 
{
	uint32_t 	ip_addr;
	uint32_t	te_id;
} tunnel_key_t;

typedef struct
{
	tunnel_key_t	key;
	pfm_ip_addr_t	remote_ip;
	tunnel_type_t	tunnel_type:2;
	uint8_t		is_row_used:1;
	union
	{
		pdus_info_t	pdus_info;
		drb_info_t	drb_info;
	};
} tunnel_t;


const tunnel_t *	tunnel_get(tunnel_key_t *key);
tunnel_t *		tunnel_add(tunnel_key_t *key);
pfm_retval_t		tunnel_remove(tunnel_key_t *key);
tunnel_t *		tunnel_modify(tunnel_key_t *key);
pfm_retval_t		tunnel_commit(tunnel_t* nt);
void 			tunnel_print_show(FILE *fp, tunnel_key_t *key);
void 			tunnel_print_list(FILE *fp, tunnel_type_t ttype);
#endif
