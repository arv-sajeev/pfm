#include <rte_timer.h>
#include "pfm.h"
#include "cuup.h"

#define ECHO_REQUEST_PERIODICITY  60 // in secs. Should greater than 59
#define T3-RESPONSE     3       // in secs.Timeout waiting for echo Response
#define N3-REQUESTS     5       // No of Echo Req retry before reporting
				// path loss err

#define MAX_GTP_PATH (MAX_DU_COUNT + MAC_UPF_COUNT)

typedef struct {
	pfm_ip_addr_t remote_ip;
	uint16_t remote_port_no;
	pfm_ip_addr_t local_ip;
	uint16_t  local_port_no;
	 struct rte_timer* echo_timer
	uint16_t tunne_count;
	uint16_t retry_count;
} gtp_path_info_t;

gtp_path_info_t gtp_path_list_g[MAX_GTP_PATH];
	

void gtp_echo_response_send(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

	/* encode Echo Respose and invke gtp_data_req() to
	   send the encoded messge to remote hose
	   can reuse the receved mbug */
}

void gtp_echo_requet_send(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

	/* Allocate mbuf
	   encode Echo Request and invke gtp_data_req() to
	   send the encoded messge to remote hose */
}

void gtp_path_add(pfm_ip_addr_t local_ip, int local_port_no,
			pfm_ip_addr_t remote_ip, int remote_port_no)
{
	/* 
	- Lookup in hash
	- If not found.
	   - Create new entry
	   - Set tunnel_count = 0
	   - Retry_count = 0
	   - Start timer ECHO_REQUEST_PERIODICITY
	- Increment tunnel_count
	*/
}

void gtp_path_del(pfm_ip_addr_t local_ip, int local_port_no,
			pfm_ip_addr_t remote_ip, int remote_port_no)
{
	- Looup in hash
	- If not found, return without doing anything
	- If found, decrement tunnel_count
	- If tunnel_count <= 0,
		- Cancel any timer if running
		- delete entry.
}


void gtp_path_echo_timer_expiry_callback(void )
//* modify above prototyp as needed for timer callback
{
 	- Increment retry_count
	- If (retry_count > N3-REQUESTS)
		- Stop running time
		- Inkoke gtp_path_err_ntfy();
	- Else
		- Send Echo Request
		- Start timer T3-RESPONSE
}

void gtp_data_req(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

	/* TODO Append GTP Header */

	pfm_data_req(	t->key.ip_addr,
			t->gtp.src_ip_addr,
			17,	// UDP 
			t->key.port_no,
			t->gtp.src_port_no,
			rte_mbuf *mbuf);

	return;
}


void pfm_data_ind(      const pfm_ip_addr_t remote_ip_addr,
			const pfm_ip_addr_t local_ip_addr,
			pfm_protocol_t protocol,
			const int remote_port_no,
			const int local_port_no,
			struct rte_mbuf *mbuf)
{
	/* TODO:

		Decode G-PDU to get Message Type and TEID
		if MsgType == Echo Request, resond with Echo Resposne
		if MsgType == Echo Respone,
		if MsgTyee == T-PDU do the following
		   - search in Hash for Tunnel Info
		   - if not found, drop packet with error
		   - if found, do the following
			get if_type from Tunnel Info
		        if ifType = NGu invoke gtp_sdapdat_ind()
			if ifTyoe - F1y inkoke gtp_pdcpdat_ind()

		remove GTP Header
		
}


