#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <rte_mbuf.h>
#include <rte_timer.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include "pfm.h"
#include "pfm_comm.h" 
#include "pfm_utils.h"
#include "cuup.h"
#include "tunnel.h"
#include "gtp.h"
#include "pdcp.h"
#include "sdap.h"


#define ECHO_REQUEST_PERIODICITY  60 // in secs. Should greater than 59
#define T3_RESPONSE     3       // in secs.Timeout waiting for echo Response
#define N3_REQUESTS     5       // No of Echo Req retry before reporting
				// path loss err
//TODO UPF count

// We are using the 3GPP TS 29.281

#define MAX_GTP_PATH (MAX_DU_COUNT + MAX_UPF_COUNT)
#define PFM_GTP_HASH_NAME "PFM_GTP_TABLE_HASH"

static gtp_table_entry_t gtp_table_g[MAX_GTP_PATH];
static struct rte_hash* gtp_table_hash_g;
static pfm_bool_t gtp_table_up_g;

// Callback declarations
static void gtp_echo_timer_wait_cb(__rte_unused struct rte_timer *timer,void *args);
static void gtp_path_echo_timer_expiry_cb(__rte_unused struct rte_timer *timer,void *args);
	
// The key of the GTP table is simply the remote ip address
// We just use it to check whether it already exists 
static pfm_retval_t
gtp_table_init(void)
{

	pfm_trace_msg("Initializing gtp_table");
	// Initialize the hash table
	struct rte_hash_parameters hash_params = 
	{
		.name			=	PFM_GTP_HASH_NAME,
		.entries 		= 	MAX_GTP_PATH,
		.reserved		= 	0,
		.key_len		=	sizeof(pfm_ip_addr_t),
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};
	gtp_table_hash_g	=	rte_hash_create(&hash_params);
	if (gtp_table_hash_g == NULL)	
	{
		pfm_log_msg(PFM_LOG_ALERT,"Error during rte_hash_create()");
		return PFM_FAILED;
	}

	int ret = rte_timer_subsystem_init();
	if (ret != 0)
	{
		pfm_log_msg(PFM_LOG_ERR,"Error during rte_timer_init()");
		return PFM_FAILED;
	}

	for (int i = 0;i < MAX_GTP_PATH;i++)
	{
		rte_timer_init(&(gtp_table_g[i].echo_timer));
	}	
	
	pfm_trace_msg("gtp_table_init complete");
	return PFM_SUCCESS;
}

static pfm_retval_t
gtp_path_clear(gtp_table_entry_t* gtp_entry)
{
	int ret;
	// Stop the timer
	ret = rte_timer_stop(&(gtp_entry->echo_timer));
	if (ret != 0)
	{
		pfm_log_msg(PFM_LOG_ERR,"rte_timer_stop() failed");
		return PFM_FAILED;
	}
	// Delete the hash key
	ret = rte_hash_del_key(gtp_table_hash_g,(void *)&(gtp_entry->gtp_path.remote_ip));
	if (ret < 0)
	{
		if (ret == -ENOENT)
		{
			pfm_log_msg(PFM_LOG_ERR,"gtp entry doesn't exist");
			return PFM_FAILED;
		}

		if (ret == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,"rte_hash_del_key() failed");
			return PFM_FAILED;
		}
	}
	// Clear the entry
	memset(&(gtp_entry->gtp_path),0,sizeof(gtp_path_info_t));
	return PFM_SUCCESS;
}

static void
gtp_echo_timer_wait_cb(__rte_unused struct rte_timer* timer,void *args)
{
	// This callback is used only got waiting for ECHO_REQUEST_PERIODICITY time and then 
	// call the gtp_path_echo_timer_expiry_cb again
	uint64_t ticks;
	gtp_table_entry_t  *gtp_entry = (gtp_table_entry_t*)args;
	ticks = ECHO_REQUEST_PERIODICITY*rte_get_timer_hz();
	
	rte_timer_reset_sync(&(gtp_entry->echo_timer),
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				gtp_path_echo_timer_expiry_cb,
				(void *)gtp_entry);
	return;
}

static void 
gtp_path_echo_timer_expiry_cb(__rte_unused struct rte_timer* timer,void *args )
{
	pfm_retval_t ret_val;
	uint64_t ticks;
	char ip_str[STR_IP_ADDR_SIZE];
	gtp_table_entry_t *gtp_entry = (gtp_table_entry_t *)args;

	// increment retry count
	gtp_entry->gtp_path.retry_count++;
	if (gtp_entry->gtp_path.retry_count > N3_REQUESTS)
	{
	// If retry_count more than N3_REQUESTS
		ret_val = gtp_path_clear(gtp_entry);
		if (ret_val != PFM_SUCCESS)
		{
			pfm_log_msg(PFM_LOG_ERR,"Error in gtp_clear_entry()");
		}
		pfm_trace_msg("cleared gtp entry :: %s",pfm_ip2str(gtp_entry->gtp_path.remote_ip,ip_str));
		// TODO gtp_path_error_notify
		return;
	}
	// Send a gtp echo request
	pfm_trace_msg("sent gtp echo request to :: %s",pfm_ip2str(gtp_entry->gtp_path.remote_ip,ip_str));
	gtp_echo_request_send(&(gtp_entry->gtp_path));
	// Wait for response
	ticks = T3_RESPONSE*rte_get_timer_hz();
	rte_timer_reset_sync(&(gtp_entry->echo_timer),
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				gtp_echo_timer_wait_cb,
				(void *)gtp_entry);
}


pfm_retval_t
gtp_echo_response_send(const gtp_path_info_t *t, struct rte_mbuf *mbuf)
{
	unsigned char* packet = rte_pktmbuf_mtod(mbuf,unsigned char*);
	packet[0] = 0x02;
	pfm_data_req(t->local_ip,t->remote_ip,PROTOCOL_UDP,t->local_port_no,t->remote_port_no,mbuf);
	return PFM_SUCCESS;
}

pfm_retval_t 
gtp_echo_request_send(gtp_path_info_t *t)
{
	char ip_str[STR_IP_ADDR_SIZE+1];
	struct rte_mbuf *mbuf;
	unsigned char* packet;
	uint32_t* tunnel_id;
	// message type 1
	mbuf = rte_pktmbuf_alloc(sys_info_g.mbuf_pool);
	if (mbuf == NULL)
	{
		pfm_log_rte_err(PFM_LOG_ERR,"rte_pktmbuf_alloc() failed in gtp_echo_request_send %s",
				pfm_ip2str(t->remote_ip,ip_str));
		return PFM_FAILED;
	}

	packet = rte_pktmbuf_mtod(mbuf,unsigned char*);
	// Version | PT | * | E | S | PN
	// 001	   | 1  | 0 | 0 | 0 | 0

	packet[0] = 0x30;
	// Message type 1
	packet[1] = 0x01;
	// Message length 0
	packet[2] = 0x00;
	packet[3] = 0x00;
	// Tunnel id
	tunnel_id = (uint32_t*)packet[4];
	tunnel_id = htonl(0);
	pfm_data_req(t->local_ip,t->remote_ip,17,t->local_port_no,t->remote_port_no,mbuf);
	pfm_trace_msg("sent gtp echo request sent :: %s",pfm_ip2str(t->remote_ip,ip_str));
	return PFM_SUCCESS;
}


const gtp_path_info_t*
gtp_path_get(pfm_ip_addr_t remote_ip)
{
	int ret;
	char ip_str[STR_IP_ADDR_SIZE+1];
	ret = rte_hash_lookup(gtp_table_hash_g,(void *)&(remote_ip));
	if (ret < 0)
	{
		if (ret == -ENOENT)
		{
			pfm_log_msg(PFM_LOG_ERR,"entry not found %s",
						pfm_ip2str(remote_ip,ip_str));
			return NULL;
		}
		pfm_log_msg(PFM_LOG_ERR,"rte_hash_lookup() failed");
		return NULL;
	}
	return &(gtp_table_g[ret].gtp_path);
}

pfm_retval_t
gtp_path_add(pfm_ip_addr_t local_ip, int local_port_no,
			pfm_ip_addr_t remote_ip, int remote_port_no)
{
	pfm_retval_t ret_val;
	int key;
	gtp_table_entry_t* gtp_entry;
	char ip_str[STR_IP_ADDR_SIZE];
	uint64_t ticks;

	// Check if the gtp_table is up
	if (gtp_table_up_g == PFM_FALSE)
	{
		ret_val = gtp_table_init();
		if (ret_val == PFM_FAILED)
		{
			pfm_log_msg(PFM_LOG_ERR,"Error in gtp_table_init()");
			return PFM_FAILED;
		}
		pfm_trace_msg("gtp_table initialized");
	}
	// Add a key for the remote ip
	key = rte_hash_add_key(gtp_table_hash_g,(void *)&remote_ip);
	if (key < 0)
	{
		if (key == -EINVAL)
		{
			pfm_log_msg(PFM_LOG_ERR,"rte_hash_add_key() failed");
		}

		else if (key == -ENOMEM)
		{
			pfm_log_msg(PFM_LOG_ERR,"gtp_path table is full");
		}
		return PFM_FAILED;
	}
	// Get entry from table with key as index
	gtp_entry 				= &(gtp_table_g[key]);
	// Populate entry
	gtp_entry->gtp_path.remote_ip 		= remote_ip;
	gtp_entry->gtp_path.remote_port_no	= remote_port_no;
	gtp_entry->gtp_path.local_ip		= local_ip;
	gtp_entry->gtp_path.local_port_no	= local_port_no;
	gtp_entry->gtp_path.tunnel_count	= 0;
	gtp_entry->gtp_path.retry_count		= 0;
	pfm_trace_msg("Added new gtp_path %s %d",
				pfm_ip2str(remote_ip,ip_str),remote_port_no);
	// Start timer
	ticks = ECHO_REQUEST_PERIODICITY*rte_get_timer_hz();
	rte_timer_reset_sync(&(gtp_entry->echo_timer),
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				gtp_path_echo_timer_expiry_cb,
				(void *)gtp_entry);

	pfm_trace_msg("Reset timer for %s %d",
				pfm_ip2str(remote_ip,ip_str),remote_port_no);
	// Set tunnel count = 1
	gtp_entry->gtp_path.tunnel_count = 1;
	/* 
	- Lookup in hash
	- If not found.
	   - Create new entry
	   - Set tunnel_count = 0
	   - Retry_count = 0
	   - Start timer ECHO_REQUEST_PERIODICITY
	- Increment tunnel_count
	*/
	return PFM_SUCCESS;
}

pfm_retval_t 
gtp_path_del(pfm_ip_addr_t remote_ip)
{
	pfm_retval_t ret_val;
	gtp_table_entry_t *gtp_entry;
	int ret;
	// Check if the entry exists
	ret = rte_hash_lookup(gtp_table_hash_g,(void *)&remote_ip);
	if (ret < 0)
	{
		if (ret == -ENOENT)
			pfm_log_msg(PFM_LOG_ALERT,"gtp path doesn,t exist");
		else if (ret == -EINVAL)
			pfm_log_msg(PFM_LOG_ALERT,"rte_hash_lookup() failed");
		return PFM_FAILED;
	}
	// Access entry and decrement tunnel count
	gtp_entry = &(gtp_table_g[ret]);
	gtp_entry->gtp_path.tunnel_count--;

	// If no tunnels reference this path clear
	if (gtp_entry->gtp_path.tunnel_count <= 0)
	{
		ret_val = gtp_path_clear(gtp_entry);
		if (ret_val != PFM_SUCCESS)
		{
			pfm_log_msg(PFM_LOG_ERR,"gtp_clear_entry() failed");
			return PFM_FAILED;
		}
	}
	return PFM_SUCCESS;
}


void 
gtp_data_req(tunnel_t *t, struct rte_mbuf *mbuf)
{
	// GTP header
	unsigned char *packet;
	char *ret;
	int pkt_len;
	uint32_t *tunnel_id;
	uint16_t *gtp_len;
	const gtp_path_info_t *gtp_entry;

	pkt_len		= mbuf->pkt_len;
	gtp_entry	= gtp_path_get(t->remote_ip);
	if (gtp_entry == NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"no valid gtp entry");
		return ;
	}

	ret = rte_pktmbuf_prepend(mbuf,GTP_HDR_SIZE);	// Header size for GTP packet is 8
	if (ret ==  NULL)
	{
		pfm_log_msg(PFM_LOG_ERR,"not enough headroom in mbuf");
		return ;
	}

	// Version and other flags

	packet = rte_pktmbuf_mtod(mbuf,unsigned char*);
	gtp_len 	= (uint16_t*)packet[2];
	tunnel_id	= (uint32_t*)packet[5];
	packet[0] 	= 0x30;					 
	// Version - 001,  PT - 1, 0,0,0,0 No extension header No sequence No N-PDU definition
	packet[1] 	= 0xFF; 
	// Value for G-PDU
	*gtp_len	= htons(pkt_len);
	*tunnel_id	= htonl(t->remote_te_id);

	pfm_data_req(t->key.ip_addr,t->remote_ip,
			PROTOCOL_UDP,gtp_entry->local_port_no,
			gtp_entry->remote_port_no,mbuf);
			
	
	return ;
}


void 
pfm_data_ind(const pfm_ip_addr_t remote_ip_addr,
	     const pfm_ip_addr_t local_ip_addr,
	     pfm_protocol_t protocol,
	     const int remote_port_no,
	     const int local_port_no,
	     struct rte_mbuf *mbuf)
{
	unsigned char* packet;
	char *ret;
	char ip_str[IP_ADDR_SIZE+1];
	uint32_t tunnel_id;
	uint16_t gtp_pload_len;
	const gtp_path_info_t *gtp_entry;
	tunnel_key_t tunnel_key;
	tunnel_t *tunnel_entry;
	

	// Assuming nothing other than GTP gets routed here
	packet 		= rte_pktmbuf_mtod(mbuf,unsigned char*);

	// Log if version and flags are different from supported
	if (packet[0] != 0x30)
		pfm_log_msg(PFM_LOG_ALERT,"incompatible GTP format :: %0X",packet[0]);
	
	// store gtp_payload length and tunnel_id
	gtp_pload_len	= ntohl(*(uint16_t*)(&(packet[2])));
	tunnel_id	= ntohl(*(uint32_t*)(&(packet[4])));
	ret = rte_pktmbuf_adj(mbuf,GTP_HDR_SIZE);
	
	if (ret == NULL)
	{
		pfm_log_msg(PFM_LOG_ALERT,"incompatible size GTP packet");
		return;
	}
	
	// switch on the message type
	switch(packet[1])
	{
		case 0x01:
			// Echo request so send an echo response
			gtp_entry = gtp_path_get(remote_ip_addr);		
			if (gtp_entry == NULL)
			{
				pfm_log_msg(PFM_LOG_ERR,"invalid gtp entry");
				break;
			}
			gtp_echo_response_send(gtp_entry,mbuf);
			break;
		case 0x02:
			// Echo response so call gtp_path_add to refresh timer
			gtp_path_add(local_ip_addr,local_port_no,remote_ip_addr,remote_port_no);
			break;

		case 0xFF:
			// T-PDU
			tunnel_key.ip_addr 	= local_ip_addr;
			tunnel_key.te_id	= tunnel_id;
			tunnel_entry 		= tunnel_get(&tunnel_key);

			if (tunnel_entry == NULL)
			{
				pfm_log_msg(PFM_LOG_ERR,"Invalid tunnel_entry %s %d",
						pfm_ip2str(local_ip_addr,ip_str),tunnel_id);
				return;
			}

			if (remote_ip_addr != tunnel_entry->remote_ip)
			{
				pfm_log_msg(PFM_LOG_ERR,"Invalid gtp path %s %d",
						pfm_ip2str(local_ip_addr,ip_str),tunnel_id);
				return;
			}

			if (tunnel_entry->tunnel_type == TUNNEL_TYPE_PDUS)
			{
				//TODO flowid 
				gtp_sdap_data_ind(tunnel_entry,10,mbuf);
				return;
			}

			if (tunnel_entry->tunnel_type == TUNNEL_TYPE_DRB)
			{
				//TODO
				gtp_pdcp_data_ind(tunnel_entry,10,mbuf);
				return;
			}
	}

}





