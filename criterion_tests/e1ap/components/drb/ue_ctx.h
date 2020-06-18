#ifndef __UE_CTX_H__
#define __UE_CTX_H__ 1

#include "tunnel.h"


typedef struct
{
	uint32_t	cuup_ue_id;    // 32 bit gNB-CU-UP UE E1AP ID
	uint32_t	cucp_ue_id; // 32 bit gNB-CU-CP UE E1AP ID
	uint8_t		pdus_count:8;       // PDU Session count
	uint8_t		drb_count:5;
	uint8_t		new_ul_flow_detected_count:5;
	pfm_bool_t	is_row_used:1;
	tunnel_t *	pdus_tunnel_list[MAX_PDUS_PER_UE];
	tunnel_t *	drb_tunnel_list[MAX_DRB_PER_UE];
} ue_ctx_t;

ue_ctx_t *		ue_ctx_add(uint32_t ue_id);
pfm_retval_t		ue_ctx_remove(uint32_t ue_id);
ue_ctx_t *		ue_ctx_modify(uint32_t ue_id);
pfm_retval_t		ue_ctx_commit(ue_ctx_t *new_ctx);
const ue_ctx_t *	ue_ctx_get(uint32_t ue_id);
void			ue_ctx_print_list(FILE *fp);
void			ue_ctx_print_show(FILE *fp, uint32_t ue_id);
pfm_retval_t 		ue_ctx_id_alloc(uint32_t *ue_id);
pfm_retval_t		ue_ctx_rollback(ue_ctx_t *ue_ctx);
#endif

