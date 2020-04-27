#ifndef __PFM_COMM_H__
#define __PFM_COMM_H__ 1

#define MAX_LINK_COUNT  5
#define LAST_LINK_ID  MAX_LINK_COUNT
#define MAX_KNI_PORTS	5
#define MAX_LOCAL_IP_COUNT	(MAX_KNI_PORTS*2)

#define	MBUF_COUNT	4096
#define MBUF_CASHE_SIZE	128
#define MBUF_PRIV_SIZE	0

/* DPDK object names */
#define	MBUF_NAME	"MBUF"
#define	TX_RING_NAME	"TXRING"
#define	RX_RING_NAME	"RXRING"
#define DIST_NAME	"DISTNAME"

#define TX_RING_SIZE    64
#define RX_RING_SIZE    TX_RING_SIZE
#define TX_BURST_SIZE   32
#define RX_BURST_SIZE   TX_BURST_SIZE

#define MAC_ADDR_SIZE	6
#define IP_ADDR_SIZE	4
#define DEFAULT_MTU_SIZE        1500
#define DEFAULT_MIN_MTU_SIZE    68
#define DEFAULT_MAX_MTU_SIZE    65535

#define ARP_TABLE_SIZE 32

enum {
	LCORE_MAIN	= 0,
	LCORE_RXLOOP	= 1,
	LCORE_TXLOOP	= 2,
	LCORE_DISTRIBUTOR	= 3,
	LCORE_WORKER	= 4,
	LCORE_MIN_LCORE_COUNT
};

typedef enum
{
	ADMSTATE_UNLOCKED,
	ADMSTATE_LOCKED,
} admin_state_t;

typedef enum
{
	OPSSTATE_NOTCONFIGURED,
	OPSSTATE_ENABLED,
	OPSSTATE_DISABLED,
	OPSSTATE_FAULTY
} ops_state_t;


typedef struct {
	uint8_t addr_bytes[IP_ADDR_SIZE];
	//uint32_t addr_number;
} ipv4_addr_t;

static inline void ipv4_addr_copy(ipv4_addr_t *ip_from,ipv4_addr_t *ip_to)	{
	
	uint8_t *from_bytes = (uint8_t *)(ip_from->addr_bytes);
	uint8_t *to_bytes   = (uint8_t *)(ip_to->addr_bytes);

	to_bytes[0] = from_bytes[0];
	to_bytes[1] = from_bytes[1];
	to_bytes[2] = from_bytes[2];
	to_bytes[3] = from_bytes[3];
}

typedef struct{
	uint8_t addr_bytes[MAC_ADDR_SIZE];
} mac_addr_t;

typedef struct {
	int kniIdx;
	ipv4_addr_t	ip_addr;
	ipv4_addr_t	network_mask;
	ipv4_addr_t	default_gateway_ip;
} local_ipv4_addr_t;

typedef struct {
	int		lcore_count;
	int		kni_count;
	int		local_ip_count;
	local_ipv4_addr_t	local_ip_addr_list[MAX_LOCAL_IP_COUNT];
	struct rte_mempool *mbuf_pool;
        struct rte_ring *rx_ring_ptr ;
        struct rte_ring *tx_ring_ptr ;
	struct rte_kni *kni_ptr;
	struct rte_distributor *dist_ptr;
} sys_info_t;


extern volatile pfm_bool_t	force_quit_g; /* all loops will use this
			varable to determin the looping need to continue
			or not. By setting this varlbale to TRUE, all loops
			can be terminated */
extern sys_info_t	sys_info_g;


#endif
