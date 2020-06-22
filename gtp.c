#include <stdio.h>
#include <string.h>
#include <rte_timer.h>
#include "pfm.h"
#include "cuup.h"
#include "tunnel.h"
#include "gtp.h"
#include <rte_timer.h>
#include <rte_hash.h>
#include <rte_jhash.h>



#define ECHO_REQUEST_PERIODICITY  60 // in secs. Should greater than 59
#define T3_RESPONSE     3       // in secs.Timeout waiting for echo Response
#define N3_REQUESTS     5       // No of Echo Req retry before reporting
				// path loss err
//TODO UPF count

#define MAX_GTP_PATH (MAX_DU_COUNT + MAX_UPF_COUNT)
#define PFM_GTP_HASH_NAME "PFM_GTP_TABLE_HASH"

static gtp_table_entry_t gtp_table_g[MAX_GTP_PATH]
static struct rte_hash* gtp_table_hash_g;
static pfm_bool_t gtp_table_up_g;
	
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

void 
gtp_echo_response_send(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

	/* encode Echo Response and invoke gtp_data_req() to
	   send the encoded message to remote host
	   can reuse the received m-buffer */
}

void 
gtp_echo_request_send(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

}

static pfm_retval_t
gtp_path_clear(gtp_table_entry_t* gtp_entry)
{
	int ret;
	// Stop the timer
	ret = rte_timer_stop(&(gtp_entry.echo_timer));
	if (ret != 0)
	{
		pfm_log_msg(PFM_LOG_ERR,"rte_timer_stop() failed");
		return PFM_FAILED;
	}
	// Delete the hash key
	ret = rte_hash_del_key(gtp_table_hash_g,(void *)&(gtp_entry->remote_ip));
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
	memset(gtp_entry.gtp_path,0,sizeof(gtp_path_info_t);
	return PFM_SUCCESS;
}

pfm_retval_t
gtp_path_add(pfm_ip_addr_t local_ip, int local_port_no,
			pfm_ip_addr_t remote_ip, int remote_port_no)
{
	pfm_retval_t ret_val;
	int ret;
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
	key = rte_hash_add_key(gtp_table_hash_g,(void *)remote_ip);
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
				gtp_path_echo_timer_expiry_callback,
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
	ret = rte_hash_lookup(gtp_table_hash_g,(void *)remote_ip);
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
	gtp_entry->tunnel_count--;

	// If no tunnels reference this path clear
	if (gtp_entry->tunnel_count <= 0)
	{
		ret_val = gtp_path_clear(gtp_entry);
		if (ret_val != PFM_SUCCESS)
		{
			pfm_log_msg(PFM_LOG_ERR,"gtp_clear_entry() failed");
			return PFM_FAILED;
		}
	}
	return PFM_SUCCESS
}

static void
gtp_echo_timer_wait_cb(__rte_unused struct rte_timer* timer,void *args)
{
	// This callback is used only got waiting for ECHO_REQUEST_PERIODICITY time and then 
	// call the gtp_path_echo_timer_expiry_cb again
	pfm_retval_t ret_val;
	int ret;
	uint64_t ticks;
	char ip_str[STR_IP_ADDR_SIZE];
	gtp_table_entry_t  *gtp_entry = (gtp_table_entry_t*)args;
	timer = ECHO_REQUEST_PERIODICITY*rte_get_timer_hz();
	
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
	int ret;
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
			pfm_log_msg("Error in gtp_clear_entry()");
		}
		// TODO gtp_path_error_notify
		return;
	}
	// Send a gtp echo request
	gtp_echo_request_send();
	// Wait for response
	ticks = T3_RESPONSE*rte_get_timer_hz();
	rte_timer_reset_sync(&(gtp_entry->echo_timer),
				ticks,
				PERIODICAL,
				rte_lcore_id(),
				gtp_echo_timer_wait_cb,
				(void *)gtp_entry);
}

void 
gtp_data_req(cuup_tunnel_info_t *t, struct rte_mbuf *mbuf)
{

	/* TODO Append GTP Header */

	pfm_data_req(t->key.ip_addr,
		t->gtp.src_ip_addr,
		17,	// UDP 
		t->key.port_no,
		t->gtp.src_port_no,
		rte_mbuf *mbuf);
	return;
}


void 
pfm_data_ind(const pfm_ip_addr_t remote_ip_addr,
	     const pfm_ip_addr_t local_ip_addr,
	     pfm_protocol_t protocol,
	     const int remote_port_no,
	     const int local_port_no,
	     struct rte_mbuf *mbuf)
{
	/* TODO:

		Decode G-PDU to get Message Type and TEID
		if MsgType == Echo Request, respond with Echo Response
		if MsgType == Echo Response,
		if MsgType == T-PDU do the following
		   - search in Hash for Tunnel Info
		   - if not found, drop packet with error
		   - if found, do the following
			get if_type from Tunnel Info
		        if ifType = NGu invoke gtp_sdapdat_ind()
			if ifType - F1u invoke gtp_pdcpdat_ind()

		Remove GTP Header
	*/
}


